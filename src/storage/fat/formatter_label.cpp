#include "openosc/storage/fat/formatter.hpp"
#include "openosc/storage/block_device.hpp"
#include "openosc/update/crc32.hpp"
#include <cstring>

namespace openosc::fat {
bool Formatter::writeVerified(uint32_t sector,uint8_t *data) {
    const auto expected=Crc32::compute(data,sectorBytes_);
    if (!disk_.write(sector,1,data,MediaOwner::Local) || !disk_.sync())
        return fail("CANNOT WRITE NEW BOOT METADATA",FR_DISK_ERR);
    if (!disk_.read(sector,1,data,MediaOwner::Local) || Crc32::compute(data,sectorBytes_)!=expected)
        return fail("NEW BOOT METADATA VERIFY FAILED",FR_DISK_ERR);
    return true;
}
bool Formatter::bootLabel() {
    // FatFs f_setlabel updates the root entry only. Keep the new volume's BPB
    // (and FAT32 backup BPB) consistent for host checkers. The volume is unmounted.
    alignas(4) uint8_t boot[4096];
    if (!disk_.read(0,1,boot,MediaOwner::Local)) return fail("CANNOT READ NEW BOOT SECTOR",FR_DISK_ERR);
    const auto word=[&](unsigned offset) { return unsigned(boot[offset])|(unsigned(boot[offset+1])<<8); };
    if (boot[510]!=0x55 || boot[511]!=0xaa || word(11)!=sectorBytes_)
        return fail("NEW BOOT SECTOR INVALID",FR_INT_ERR);
    const bool fat32=!word(22);
    const unsigned backup=fat32 ? word(50) : 0;
    const unsigned info=fat32 ? word(48) : 0;
    if (fat32 && (!backup || !info || backup+info>=word(14) || backup+info>=sectors_))
        return fail("NEW BACKUP BOOT SECTOR INVALID",FR_INT_ERR);
    char *label=reinterpret_cast<char*>(boot+(fat32 ? 71 : 43));
    std::memset(label,' ',11);
    std::memcpy(label,VolumeLabel,std::strlen(VolumeLabel));
    if (!writeVerified(0,boot) || (backup && !writeVerified(backup,boot))) return false;
    if (backup) {
        if (!disk_.read(backup+info,1,boot,MediaOwner::Local)) return fail("CANNOT READ BACKUP FSINFO",FR_DISK_ERR);
        // FatFs maintains only the primary FSInfo. The backup's allocation hints
        // are advisory: initialize them as unknown so ordinary file writes do not
        // leave a stale exact free count in the backup immediately after format.
        std::memset(boot+488,0xff,8);
        if (!writeVerified(backup+info,boot)) return false;
    }
    return true;
}
} // namespace openosc::fat
