#include "openosc/platform/usb_core.hpp"
#include "stm32h7xx_hal.h"

namespace openosc {
// noinline: the RCC macros' read-back locals stay out of the callers' frames (Board::initialize's
// stack layout), also under LTO.
__attribute__((noinline)) void UsbCore::powerOff() {
    __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
    reinterpret_cast<USB_OTG_DeviceTypeDef *>(USB1_OTG_HS_PERIPH_BASE + USB_OTG_DEVICE_BASE)->DCTL |= USB_OTG_DCTL_SDIS;
    USB1_OTG_HS->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;
    __HAL_RCC_USB1_OTG_HS_FORCE_RESET();
    __DSB();
    __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
    __HAL_RCC_USB1_OTG_HS_CLK_DISABLE();
    NVIC_DisableIRQ(OTG_HS_IRQn); NVIC_ClearPendingIRQ(OTG_HS_IRQn); // Anything raised before the reset.
}
} // namespace openosc
