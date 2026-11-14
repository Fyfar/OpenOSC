#pragma once
#include "stm32h7xx.h"

namespace openosc {
// SYSRESETREQ written directly, then a loop in place that feeds nothing: if the request did not take,
// the unfed IWDG resets. Inline only, so fatal paths (fault handlers included) call no function.
struct SystemReset {
    [[noreturn]] __attribute__((always_inline)) static inline void now() {
        __DSB();
        SCB->AIRCR = (0x5faUL << SCB_AIRCR_VECTKEY_Pos) | (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) | SCB_AIRCR_SYSRESETREQ_Msk;
        __DSB();
        while (true) __NOP();
    }
};
} // namespace openosc
