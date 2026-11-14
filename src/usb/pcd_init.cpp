#include "openosc/usb/usb_device.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/usb/supply_ready.hpp"

extern "C" void HAL_PCD_MspInit(PCD_HandleTypeDef* handle) {
#ifndef OPENOSC_APPLICATION
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection = RCC_PERIPHCLK_USB;
    clock.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) return;
#endif
    HAL_PWREx_EnableUSBVoltageDetector();
    __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
#ifdef OPENOSC_APPLICATION
    __HAL_RCC_USB1_OTG_HS_FORCE_RESET();
    __DSB();
    __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
#endif
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_OTG1_HS;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 4, 0); // Control/data completions precede acquisition bookkeeping.
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
}

extern "C" USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* device) {
    auto& pcd = openosc::usbPcd;
    pcd = {};
    pcd.Instance = USB1_OTG_HS;
    pcd.Init.dev_endpoints = 9;
    pcd.Init.speed = PCD_SPEED_FULL;
    pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
    pcd.Init.ep0_mps = EP_MPS_64;
#ifdef OPENOSC_APPLICATION
    pcd.Init.dma_enable = ENABLE;
#else
    pcd.Init.dma_enable = DISABLE;
#endif
    pcd.Init.vbus_sensing_enable = DISABLE;
    pcd.pData = device;
    device->pData = &pcd;
    // VDD33USB from the board (USBREGEN stays 0): its detector, then USB33RDY, before the PHY
    // is used. 10 ms, or 2M register reads should the tick not advance.
    HAL_PWREx_EnableUSBVoltageDetector();
    if (!openosc::usb::supplyReady([] { return (PWR->CR3 & PWR_CR3_USB33RDY) != 0; }, HAL_GetTick, 10, 2000000))
        return USBD_FAIL;
    if (HAL_PCD_Init(&pcd) != HAL_OK
        || HAL_PCDEx_SetRxFiFo(&pcd, 256) != HAL_OK
        || HAL_PCDEx_SetTxFiFo(&pcd, 0, 64) != HAL_OK
        || HAL_PCDEx_SetTxFiFo(&pcd, 1, 128) != HAL_OK) return USBD_FAIL;
#ifdef OPENOSC_APPLICATION
    if (HAL_PCDEx_SetTxFiFo(&pcd,2,16) != HAL_OK || HAL_PCDEx_SetTxFiFo(&pcd,3,128) != HAL_OK) return USBD_FAIL;
#endif
    return USBD_OK;
}
#ifdef OPENOSC_USB_TRACE
extern "C" __attribute__((used)) void otgIrqEntry(const uint32_t *frame) {
    openosc::usbtrace::irq(USB1_OTG_HS->GINTSTS & USB1_OTG_HS->GINTMSK, USB1_OTG_HS->GINTSTS, frame);
    HAL_PCD_IRQHandler(&openosc::usbPcd);
}
extern "C" __attribute__((naked)) void OTG_HS_IRQHandler() {
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n b otgIrqEntry\n");
}
#else
extern "C" void OTG_HS_IRQHandler() { HAL_PCD_IRQHandler(&openosc::usbPcd); }
#endif

