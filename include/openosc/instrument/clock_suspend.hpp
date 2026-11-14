#pragma once
#include <cstdint>

namespace openosc {
// DataAccess::suspendClockUsers, before a core clock change.
enum class ClockSuspend : uint8_t {
    Suspended, // Every PLL1-timed user stopped: the clock may change.
    Busy,      // Refused; nothing changed (all checks pass before anything stops, or what stopped ran again).
    Disrupted, // A user did not stop and one that had stopped did not start again: the clock is unchanged.
};
} // namespace openosc
