#include "openosc/usb/usb_device.hpp"
#include "openosc/platform/board.hpp"
#include <cstring>

namespace {
openosc::BlockDevice* storage = nullptr;
alignas(4) uint8_t classMemory[sizeof(USBD_MSC_BOT_HandleTypeDef)];
bool allocated = false;
bool ready(uint8_t lun) {
    return !lun && storage && storage->owner() == openosc::MediaOwner::Host;
}
int8_t initialize(uint8_t lun) {
    if (!ready(lun)) return -1;
    auto* state = static_cast<USBD_MSC_BOT_HandleTypeDef*>(openosc::usbDevice.pClassDataCmsit[0]);
    if (!state) return -1;
    state->scsi_blk_nbr = storage->sectorCount();
    state->scsi_blk_size = storage->sectorSize();
    return 0;
}
int8_t capacity(uint8_t lun, uint32_t* count, uint16_t* size) {
    if (!ready(lun) || !count || !size) return -1;
    *count = storage->sectorCount();
    *size = storage->sectorSize();
    return 0;
}
int8_t isReady(uint8_t lun) { return ready(lun) ? 0 : -1; }
int8_t isProtected(uint8_t lun) { return ready(lun) ? 0 : -1; }
int8_t read(uint8_t lun, uint8_t* data, uint32_t address, uint16_t count) {
    openosc::usbHostClaimed = true;
    if (!ready(lun)) return -1;
    bool result = storage->read(address, count, data, openosc::MediaOwner::Host);
    if (result) openosc::boot_diagnostics.usbReads += count;
    return result ? 0 : -1;
}
int8_t write(uint8_t lun, uint8_t* data, uint32_t address, uint16_t count) {
    openosc::usbHostClaimed = true;
    if (!ready(lun)) return -1;
    bool result = storage->write(address, count, data, openosc::MediaOwner::Host);
    if (result) openosc::boot_diagnostics.usbWrites += count;
    return result ? 0 : -1;
}
int8_t maxLun() { return 0; }
int8_t inquiry[36] = {
    0, int8_t(0x80), 2, 2, 31, 0, 0, 0,
    'O','p','e','n','O','S','C',' ',
    'R','e','c','o','v','e','r','y',' ','F','l','a','s','h',' ',' ',
    '0','1','0','0'
};
}
namespace openosc {
void setUsbStorage(BlockDevice* disk) { storage = disk; }
USBD_StorageTypeDef recoveryStorage = {initialize, capacity, isReady, isProtected, read, write, maxLun, inquiry};
}
extern "C" void* recovery_usb_allocate(uint32_t size) {
    if (allocated || size > sizeof classMemory) return nullptr;
    allocated = true;
    std::memset(classMemory, 0, sizeof classMemory);
    return classMemory;
}
extern "C" void recovery_usb_free(void* pointer) {
    if (pointer == classMemory) allocated = false;
}
extern "C" void recovery_usb_delay(uint32_t ms) { openosc::Board::delay(ms); }
extern "C" int recovery_usb_sync(uint8_t lun) { return ready(lun) && storage->sync() ? 0 : -1; }

