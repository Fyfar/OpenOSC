#pragma once
#include "openosc/power/battery.hpp"
#include "stm32h7xx_hal.h"

namespace openosc {
class BatteryMonitor {
public:
    bool initialize();
    void poll(uint32_t now);
    const power::BatteryState &state() const { return gauge_.state(); }
private:
    ADC_HandleTypeDef adc_{};
    power::BatteryGauge gauge_;
    uint32_t next_=0,started_=0;
    bool ready_=false,pending_=false;
};
} // namespace openosc
