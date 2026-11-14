#pragma once
#include <cstdint>

namespace openosc {
struct PerformanceSnapshot {
    uint32_t uptimeMs = 0, coreHz = 0, captures = 0, projections = 0, flushes = 0;
    uint64_t elapsedCycles = 0, uiCycles = 0;
};
// Foreground loop intervals must stay below one DWT wrap (21 s at 200 MHz).
class PerformanceCounter {
public:
    void start(uint32_t cycles) { previous_ = cycles; elapsed_ = ui_ = 0; }
    void tick(uint32_t cycles) { elapsed_ += uint32_t(cycles - previous_); previous_ = cycles; }
    void ui(uint32_t cycles) { ui_ += cycles; }
    uint64_t elapsed() const { return elapsed_; }
    uint64_t ui() const { return ui_; }
private:
    uint32_t previous_ = 0;
    uint64_t elapsed_ = 0, ui_ = 0;
};
}
