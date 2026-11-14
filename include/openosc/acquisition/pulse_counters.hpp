#pragma once
#include "openosc/acquisition/counter_window.hpp"
#include "openosc/platform/resources.hpp"

namespace openosc::scope {
class PulseCounters {
public:
    explicit PulseCounters(PeripheralResources &resources):resources_(resources) {}
    ~PulseCounters() { stop(); }
    bool start();
    void stop();
    void invalidate(uint32_t now);
    void service(uint32_t now);
    const CounterSnapshot &snapshot() const { return window_.snapshot(); }
private:
    uint32_t read(uint32_t (&counts)[2]) const;
    static void filter(uint8_t level); // Both counters' ETR filter (CounterLadder::Filters).
    uint32_t kernelHz_=0; // f_CK_INT of TIM2/TIM4.
    static void interrupt();
    static PulseCounters *active_;
    PeripheralResources &resources_;
    PeripheralLease lease_;
    CounterWindow window_;
    uint32_t polledAt_=0;
    volatile uint32_t high_=0;
};
} // namespace openosc::scope
