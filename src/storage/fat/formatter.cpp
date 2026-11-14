#include "openosc/storage/fat/formatter.hpp"
#include "openosc/storage/filesystem.hpp"
#include <cstring>

namespace openosc::fat {
bool Formatter::fail(const char *error,FRESULT result) {
    error_=error; result_=result; state_=State::Failed;
    return false;
}
bool Formatter::begin() {
    if (busy()) return false;
    error_=nullptr; result_=FR_OK;
    if (disk_.owner()!=MediaOwner::Local) return fail("USB STILL OWNS THE DISK",FR_NOT_READY);
    sectors_=disk_.sectorCount(); sectorBytes_=disk_.sectorSize();
    if (sectorBytes_<512 || sectorBytes_>4096 || (sectorBytes_&(sectorBytes_-1)) ||
        uint64_t(sectors_)*sectorBytes_<4*1024*1024ULL || uint64_t(sectors_)*sectorBytes_>0xffffffffULL)
        return fail("UNSUPPORTED DISK GEOMETRY",FR_INVALID_PARAMETER);
    if (!fs_.unmount() || !disk_.sync()) return fail("CANNOT FINISH DISK OPERATIONS",FR_DISK_ERR);
    registerFatDevice(&disk_);
    state_=State::Filesystem;
    return true;
}
void Formatter::service() {
    if (!busy()) return;
    if (disk_.owner()!=MediaOwner::Local || disk_.sectorSize()!=sectorBytes_ || disk_.sectorCount()!=sectors_) {
        fail("DISK OWNERSHIP OR GEOMETRY CHANGED",FR_NOT_READY); return;
    }
    if (state_==State::Filesystem) {
        // Whole local medium as one superfloppy (no partition table: a previous one is replaced), two FAT
        // copies, data area and clusters aligned to the NOR's 4 KiB erase sector. Clusters of 4 KiB first;
        // f_mkfs does not choose another size for a given one, so where its cluster count falls between
        // FAT12 and FAT16 after the metadata (16 MiB in 4096-byte sectors: 4096 estimated, 4085 left) it
        // refuses before writing anything (FR_MKFS_ABORTED) and the next erase-aligned size is tried.
        alignas(4) uint8_t workspace[4096];
        FRESULT result=FR_MKFS_ABORTED;
        for (const UINT cluster : ClusterBytes) {
            if (cluster<sectorBytes_) continue;
            const MKFS_PARM parameters={FM_FAT|FM_FAT32|FM_SFD,2,EraseBytes/sectorBytes_,512,cluster};
            result=f_mkfs("",&parameters,workspace,sizeof workspace);
            if (result!=FR_MKFS_ABORTED) break;
        }
        if (result!=FR_OK) { fail("CANNOT CREATE FAT VOLUME",result); return; }
        state_=State::Directories;
    } else if (state_==State::Directories) {
        if (!fs_.mountPrepared()) { fail("CANNOT CREATE STANDARD DIRECTORIES",fs_.result()); return; }
        const auto result=f_setlabel(VolumeLabel);
        if (result!=FR_OK) { fail("CANNOT SET VOLUME LABEL",result); return; }
        if (!fs_.unmount()) { fail("CANNOT SYNC NEW VOLUME",FR_DISK_ERR); return; }
        state_=State::Verify;
    } else {
        if (!bootLabel()) return;
        if (!fs_.mount()) { fail("CANNOT REMOUNT NEW VOLUME",fs_.result()); return; }
        char label[12]{};
        const auto result=f_getlabel("",label,nullptr);
        if (result!=FR_OK || std::strcmp(label,VolumeLabel)) { fail("VOLUME LABEL VERIFY FAILED",result==FR_OK ? FR_INT_ERR : result); return; }
        if (!fs_.refreshFree() || !fs_.freeBytes() || !disk_.sync()) { fail("VOLUME VERIFY OR SYNC FAILED",FR_DISK_ERR); return; }
        state_=State::Complete;
    }
}
const char *Formatter::phase() const {
    switch (state_) {
    case State::Filesystem: return "CREATING FAT VOLUME";
    case State::Directories: return "CREATING DIRECTORIES";
    case State::Verify: return "VERIFYING VOLUME";
    case State::Complete: return "FORMAT COMPLETE";
    default: return "FORMAT NOT RUNNING";
    }
}
unsigned Formatter::completedSteps() const {
    return state_==State::Directories ? 1 : state_==State::Verify ? 2 : state_==State::Complete ? 3 : 0;
}
} // namespace openosc::fat
