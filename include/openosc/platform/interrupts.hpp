#pragma once
#include "stm32h7xx.h"

namespace openosc {
class Interrupts {
  public:
    using Handler = void (*)();
    static bool attach(IRQn_Type irq, Handler handler, unsigned priority = 5);
    static void detach(IRQn_Type irq);
};
class TimerClock {
  public:
    static uint32_t apb1();
    static uint32_t apb2();
};
} // namespace openosc
