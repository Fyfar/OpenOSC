#include "openosc/platform/board.hpp"
extern "C" {
extern uint32_t _estack, _sitext, _stext, _etext, _sidata, _sdata, _edata, _sbss, _ebss;
#ifdef OPENOSC_APPLICATION
extern uint32_t _sidma_data, _sdma_data, _edma_data;
#endif
#ifdef OPENOSC_USB_TRACE
extern uint64_t _susb_trace, _eusb_trace; // The ITCM trace ring (.usb_trace), 8-byte aligned.
#endif
void Reset_Handler();
void Default_Handler();
void HardFault_Handler();
void SysTick_Handler();
void OTG_HS_IRQHandler();
int main();
void __libc_init_array();
void _init() {}
__attribute__((aligned(1024))) uint32_t runtime_vectors[256];
extern const uint32_t vectors[171];
__attribute__((section(".bootstrap"), noinline, optimize("no-tree-loop-distribute-patterns")))
void Reset_Handler() {
    __disable_irq();
    SCB->CPACR |= 0x00f00000;
    __DSB(); __ISB();
    if (SCB->CCR & SCB_CCR_DC_Msk) SCB_DisableDCache();
    if (SCB->CCR & SCB_CCR_IC_Msk) SCB_DisableICache();
    MPU->CTRL = 0;
    __DSB(); __ISB();
#ifndef OPENOSC_APPLICATION
    auto* source = &_sitext;
    for (auto* p = &_stext; p < &_etext; ++p) *p = *source++;
#endif
    auto* sourceData = &_sidata;
    for (auto* p = &_sdata; p < &_edata; ++p) *p = *sourceData++;
#ifdef OPENOSC_APPLICATION
    auto* sourceDma = &_sidma_data;
    for (auto* p = &_sdma_data; p < &_edma_data; ++p) *p = *sourceDma++;
#endif
    for (auto* p = &_sbss; p < &_ebss; ++p) *p = 0;
#ifdef OPENOSC_USB_TRACE
    // Before any trace hook: ITCM enabled, with read-modify-write for its ECC (the ring's word
    // stores are narrower than an ECC word), and every ECC word of the ring written once.
    SCB->ITCMCR |= SCB_ITCMCR_EN_Msk | SCB_ITCMCR_RMW_Msk;
    __DSB(); __ISB();
    for (auto* p = &_susb_trace; p < &_eusb_trace; ++p) *p = 0;
    __DSB();
#endif
    for (unsigned i = 0; i < 171; ++i)
        runtime_vectors[i] = reinterpret_cast<const uint32_t*>(vectors)[i];
    SCB->VTOR = reinterpret_cast<uint32_t>(runtime_vectors);
    __DSB(); __ISB();
    __libc_init_array();
    main();
    for (;;) {}
}
void Default_Handler() {
    openosc::boot_diagnostics.error = 0x10000000 | (SCB->ICSR & 0x1ff);
    while (true) openosc::Board::service();
}
__attribute__((used)) void fault_capture(uint32_t* frame, uint32_t lr) {
    openosc::boot_diagnostics.stage = 0xdead;
    openosc::boot_diagnostics.faultPc = frame[6];
    openosc::boot_diagnostics.faultLr = lr;
    openosc::boot_diagnostics.faultCfsr = SCB->CFSR;
    while (true) openosc::Board::service();
}
__attribute__((naked)) void HardFault_Handler() {
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n mov r1, lr\n b fault_capture");
}
}

