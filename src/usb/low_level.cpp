#include "openosc/usb/usb_device.hpp"
#include "openosc/platform/board.hpp"

namespace {
PCD_HandleTypeDef* pcd(USBD_HandleTypeDef* device) { return static_cast<PCD_HandleTypeDef*>(device->pData); }
USBD_StatusTypeDef status(HAL_StatusTypeDef value) {
    return value == HAL_OK ? USBD_OK : value == HAL_BUSY ? USBD_BUSY : USBD_FAIL;
}
// A PCD whose initialization stopped before HAL_PCD_Init (USB33RDY not ready: no MSP clocks,
// the controller's registers not clocked) has nothing to stop: USBD_DeInit calls Stop, then
// the class and DeInit, also after a failed USBD_Init.
bool initialized(USBD_HandleTypeDef* device) {
    const auto *handle = pcd(device);
    return handle && handle->State != HAL_PCD_STATE_RESET;
}
}
extern "C" USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef* device) {
    return initialized(device) ? status(HAL_PCD_DeInit(pcd(device))) : USBD_OK;
}
extern "C" USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef* device) { return status(HAL_PCD_Start(pcd(device))); }
extern "C" USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef* device) {
    return initialized(device) ? status(HAL_PCD_Stop(pcd(device))) : USBD_OK;
}
extern "C" USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef* device, uint8_t ep, uint8_t type, uint16_t size) {
    return status(HAL_PCD_EP_Open(pcd(device), ep, size, type));
}
extern "C" USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* device, uint8_t ep) { return status(HAL_PCD_EP_Close(pcd(device), ep)); }
extern "C" USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* device, uint8_t ep) { return status(HAL_PCD_EP_Flush(pcd(device), ep)); }
extern "C" USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* device, uint8_t ep) { return status(HAL_PCD_EP_SetStall(pcd(device), ep)); }
extern "C" USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* device, uint8_t ep) { return status(HAL_PCD_EP_ClrStall(pcd(device), ep)); }
extern "C" uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef* device, uint8_t ep) {
    return ep & 0x80 ? pcd(device)->IN_ep[ep & 0x7f].is_stall : pcd(device)->OUT_ep[ep & 0x7f].is_stall;
}
extern "C" USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef* device, uint8_t address) { return status(HAL_PCD_SetAddress(pcd(device), address)); }
extern "C" USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* device, uint8_t ep, uint8_t* data, uint32_t size) {
    __DMB(); // Publish uncached DMA data before enabling the USB endpoint.
    return status(HAL_PCD_EP_Transmit(pcd(device), ep, data, size));
}
extern "C" USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef* device, uint8_t ep, uint8_t* data, uint32_t size) {
    return status(HAL_PCD_EP_Receive(pcd(device), ep, data, size));
}
extern "C" uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef* device, uint8_t ep) { return HAL_PCD_EP_GetRxCount(pcd(device), ep); }
extern "C" void USBD_LL_Delay(uint32_t ms) { openosc::Board::delay(ms); }
