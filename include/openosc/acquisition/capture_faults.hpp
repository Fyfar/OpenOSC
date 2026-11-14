#pragma once
#include <cstdint>

// Every capture fault (the values usbtrace::Tag::CaptureFault documents), counted from boot in every
// build. The trace ring has gaps between a read and the next arm and can overflow; the HIL harness
// reads this counter over SWD before and after an interval instead, and arming never resets it.
extern "C" struct OpenoscCaptureFaults {
    volatile uint32_t count, last;
} openosc_capture_faults;

namespace openosc::scope {
// Counts `value` first, then marks the trace: the count holds even when the ring overwrites the mark.
void captureFault(uint32_t value);
} // namespace openosc::scope
