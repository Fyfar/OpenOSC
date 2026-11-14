#include "openosc/usb/usb_device.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/usb_trace.hpp"

namespace {
USBD_HandleTypeDef* device(PCD_HandleTypeDef* handle) { return static_cast<USBD_HandleTypeDef*>(handle->pData); }
}
extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* handle) {
    __DMB(); // Acquire the SETUP packet written by the USB DMA master.
#ifdef OPENOSC_APPLICATION
    ++openosc::boot_diagnostics.usbReads; // Control requests, useful even before class configuration.
    // A new SETUP aborts the previous EP0 transaction. The HAL dispatches OUT
    // before IN: an old IN XFRC must not complete the response we are about to arm.
    auto *in = reinterpret_cast<USB_OTG_INEndpointTypeDef*>(reinterpret_cast<uintptr_t>(handle->Instance) + USB_OTG_IN_ENDPOINT_BASE);
    in->DIEPINT = USB_OTG_DIEPINT_XFRC;
    if (!handle->Init.dma_enable) {
        auto *deviceRegisters = reinterpret_cast<USB_OTG_DeviceTypeDef*>(reinterpret_cast<uintptr_t>(handle->Instance) + USB_OTG_DEVICE_BASE);
        deviceRegisters->DIEPEMPMSK &= ~1U;
        HAL_PCD_EP_Flush(handle,0x80);
    }
#endif
    auto* setup = reinterpret_cast<uint8_t*>(handle->Setup);
    if ((setup[0] & 31U) == USB_REQ_RECIPIENT_ENDPOINT
        && (setup[5] || (setup[4] & 0x70U) || setup[4] == 2U || (setup[4] & 15U) >
#ifdef OPENOSC_APPLICATION
            3U
#else
            1U
#endif
            )) {
        HAL_PCD_EP_SetStall(handle, 0x80);
        HAL_PCD_EP_SetStall(handle, 0);
        return;
    }
    USBD_LL_SetupStage(device(handle), setup);
    if (device(handle)->dev_state == USBD_STATE_CONFIGURED) openosc::usbHostClaimed = true;
}
extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* handle, uint8_t ep) {
    __DMB(); // Observe completed USB DMA writes before publishing received data.
    openosc::usbtrace::mark(openosc::usbtrace::Tag::Out, ep | (handle->OUT_ep[ep].xfer_count << 8));
    USBD_LL_DataOutStage(device(handle), ep, handle->OUT_ep[ep].xfer_buff);
    if (ep == 1 || ep == 3) openosc::usbLastActivity = HAL_GetTick();
}
extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* handle, uint8_t ep) {
    USBD_LL_DataInStage(device(handle), ep, handle->IN_ep[ep].xfer_buff);
    if (ep == 1 || ep == 3) openosc::usbLastActivity = HAL_GetTick();
}
extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* handle) {
#ifdef OPENOSC_APPLICATION
    ++openosc::usbResets;
#endif
    USBD_LL_SetSpeed(device(handle), USBD_SPEED_FULL);
    USBD_LL_Reset(device(handle));
}
extern "C" void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* handle) { USBD_LL_Suspend(device(handle)); }
extern "C" void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* handle) { USBD_LL_Resume(device(handle)); }
extern "C" void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* handle) { USBD_LL_DevConnected(device(handle)); }
extern "C" void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* handle) { USBD_LL_DevDisconnected(device(handle)); }
extern "C" void HAL_PCD_SOFCallback(PCD_HandleTypeDef* handle) { USBD_LL_SOF(device(handle)); }
extern "C" void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef* handle, uint8_t ep) { USBD_LL_IsoOUTIncomplete(device(handle), ep); }
extern "C" void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef* handle, uint8_t ep) { USBD_LL_IsoINIncomplete(device(handle), ep); }

