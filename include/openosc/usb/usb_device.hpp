#pragma once
#include "openosc/storage/block_device.hpp"
#include "usbd_core.h"
#include "usbd_msc.h"

namespace openosc {
extern USBD_HandleTypeDef usbDevice;
extern PCD_HandleTypeDef usbPcd;
extern volatile bool usbHostClaimed;
extern volatile uint32_t usbLastActivity;
extern volatile uint32_t usbResets;
extern USBD_DescriptorsTypeDef recoveryDescriptors;
extern USBD_StorageTypeDef recoveryStorage;
void setUsbStorage(BlockDevice* disk);

class UsbMassStorage {
public:
    explicit UsbMassStorage(BlockDevice& disk) : disk_(disk) {}
    bool start();
    bool stop();
    bool hostReleased();
    bool active() const { return active_; }
    uint32_t state() const;
private:
    BlockDevice& disk_;
    bool active_ = false;
    uint32_t ejectedSince_ = 0;
};
}

