#pragma once
#include "openosc/data/status.hpp"

namespace openosc {
struct TimerPlan {
    uint16_t prescaler = 0, reload = 0;
    Rational actualHz;
    static bool calculate(uint32_t clockHz, Rational requestedHz, TimerPlan &output,
                          uint32_t minimumPeriod = 1);
};
} // namespace openosc
