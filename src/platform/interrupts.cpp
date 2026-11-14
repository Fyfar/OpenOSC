#include "openosc/platform/interrupts.hpp"
#include "openosc/platform/board.hpp"

extern "C" uint32_t runtime_vectors[256];
extern "C" void Default_Handler();
namespace openosc {
bool Interrupts::attach(IRQn_Type irq, Handler handler, unsigned priority) {
    if (int(irq) < 0 || unsigned(irq) >= 155 || !handler)
        return false;
    NVIC_DisableIRQ(irq);
    runtime_vectors[16 + unsigned(irq)] = reinterpret_cast<uint32_t>(handler);
    __DSB();
    __ISB();
    NVIC_SetPriority(irq, priority);
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);
    return true;
}
void Interrupts::detach(IRQn_Type irq) {
    if (int(irq) < 0 || unsigned(irq) >= 155)
        return;
    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    runtime_vectors[16 + unsigned(irq)] = reinterpret_cast<uint32_t>(Default_Handler);
    __DSB();
    __ISB();
}
uint32_t TimerClock::apb1() {
    const uint32_t clock = HAL_RCC_GetPCLK1Freq();
    const unsigned encoding = (RCC->CDCFGR2 & RCC_CDCFGR2_CDPPRE1) >> RCC_CDCFGR2_CDPPRE1_Pos;
    if (!(RCC->CFGR & RCC_CFGR_TIMPRE))
        return encoding < 4 ? clock : clock * 2;
    return encoding <= 5 ? HAL_RCC_GetHCLKFreq() : clock * 4;
}
uint32_t TimerClock::apb2() {
    const uint32_t clock = HAL_RCC_GetPCLK2Freq();
    const unsigned encoding = (RCC->CDCFGR2 & RCC_CDCFGR2_CDPPRE2) >> RCC_CDCFGR2_CDPPRE2_Pos;
    if (!(RCC->CFGR & RCC_CFGR_TIMPRE))
        return encoding < 4 ? clock : clock * 2;
    return encoding <= 5 ? HAL_RCC_GetHCLKFreq() : clock * 4;
}
} // namespace openosc
