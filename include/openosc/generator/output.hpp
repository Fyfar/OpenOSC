#pragma once
#include "openosc/generator/waveform.hpp"
#include "openosc/platform/resources.hpp"

namespace openosc::generator {
class Output {
public:
    explicit Output(PeripheralResources &resources) : resources_(resources) {}
    ~Output() { disable(); }
    bool configure(const Settings &settings);
    bool enable();
    void disable();
    void service();
    // After a core clock change, with the output disabled: the timer plan is computed again from
    // the new APB1 timer clock for the same settings, and the output enabled again if `enable`.
    bool retime(bool enable) { configured_ = false; return !enable || this->enable(); }
    // A run-mode change: TIM6/TIM13 run from PLL1, so the output stops and retime() plans it again.
    bool suspendForClock() { disable(); return !enabled_; }
    bool clockBusy() const { return false; }
    bool offConfirmed() const { return !enabled_; } // The DAC/PWM halted is off.
    bool enabled() const { return enabled_; }
    bool busy() const { return false; }
    // BDMA to the DAC shares the AHB with the GPIOD reads: at full speed requests are lost and the
    // record overruns at no fixed DAC rate (1 kHz fails, 1.5 kHz does not), so capture slows down
    // whenever the DAC streams. PWM output needs no DMA.
    bool loadsBus() const { return enabled_ && !plan_.pwm; }
    DataStatus status() const { return status_; }
    const Settings &settings() const { return settings_; }
    const OutputPlan &plan() const { return plan_; }
    Snapshot snapshot() const { return {settings_, plan_, status_, enabled_}; }

private:
    bool apply();
    bool halt();
    PeripheralResources &resources_;
    PeripheralLease lease_;
    Settings settings_;
    OutputPlan plan_;
    unsigned activeBank_ = 0;
    bool configured_ = false, enabled_ = false;
    DataStatus status_ = DataStatus::NoData;
};
} // namespace openosc::generator
