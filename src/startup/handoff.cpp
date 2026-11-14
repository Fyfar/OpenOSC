#include "openosc/platform/board.hpp"
#include "openosc/platform/handoff.hpp"
namespace openosc {
[[noreturn]] void Handoff::jump(uint32_t vectorAddress) {
    __disable_irq();
    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (unsigned n = 0; n < 8; ++n) { NVIC->ICER[n] = 0xffffffff; NVIC->ICPR[n] = 0xffffffff; }
    SCB->VTOR = vectorAddress; __DSB(); __ISB();
    uint32_t sp = *reinterpret_cast<const uint32_t*>(vectorAddress);
    uint32_t entry = *reinterpret_cast<const uint32_t*>(vectorAddress + 4);
    __asm volatile("msr control, %0\nmsr basepri, %0\nmsr faultmask, %0\n"
                   "msr msp, %1\ncpsie i\nbx %2" :: "r"(0), "r"(sp), "r"(entry) : "memory");
    __builtin_unreachable();
}
}
