#include "openosc/storage/block_device.hpp"
#include "ff.h"
#include "diskio.h"

namespace {
openosc::BlockDevice* disk = nullptr;
bool local(BYTE drive) { return !drive && disk && disk->owner() == openosc::MediaOwner::Local; }
}
namespace openosc {
void registerFatDevice(BlockDevice* device) { disk = device; }
}
extern "C" DSTATUS disk_initialize(BYTE drive) { return local(drive) ? 0 : STA_NOINIT; }
extern "C" DSTATUS disk_status(BYTE drive) { return local(drive) ? 0 : STA_NOINIT; }
extern "C" DRESULT disk_read(BYTE drive, BYTE* data, LBA_t lba, UINT count) {
    if (!local(drive)) return RES_NOTRDY;
    return disk->read(lba, count, data, openosc::MediaOwner::Local) ? RES_OK : RES_ERROR;
}
extern "C" DRESULT disk_write(BYTE drive, const BYTE* data, LBA_t lba, UINT count) {
    if (!local(drive)) return RES_NOTRDY;
    return disk->write(lba, count, data, openosc::MediaOwner::Local) ? RES_OK : RES_ERROR;
}
extern "C" DRESULT disk_ioctl(BYTE drive, BYTE command, void* output) {
    if (!local(drive)) return RES_NOTRDY;
    switch (command) {
    case CTRL_SYNC: return disk->sync() ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
        if (!output) return RES_PARERR;
        *static_cast<LBA_t*>(output) = disk->sectorCount();
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (!output) return RES_PARERR;
        *static_cast<WORD*>(output) = disk->sectorSize();
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (!output) return RES_PARERR;
        *static_cast<DWORD*>(output) = 4096 / disk->sectorSize();
        return RES_OK;
    default: return RES_PARERR;
    }
}

