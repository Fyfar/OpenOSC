#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
// The application's stack grows down through DTCM towards the end of .bss (_ebss), where the last
// members of DataAccess lie (its initialized_/active_/stopping_ flags about 0.1 KiB below _ebss, the
// held frame lease 0.6 KiB below). A stack that ran past _ebss once corrupted them silently: the
// measurement hardware refused every mode change and DMM command after the first-install theme was
// written (GitHub issue #8), or a later RUN faulted on the lease (Reload settings). The static check
// (scripts/test_stack_budget.py) keeps every known chain above the guard; this is the runtime net:
// the lowest Words of the stack hold Pattern from boot, the main loop checks them every pass, and a
// broken pattern is recorded (ResetRecord StackOverflow, docs/WATCHDOG.md) and resets the image
// through the watchdog instead of running on with corrupted state.
class StackGuard {
public:
    static constexpr size_t Words = 64; // 256 bytes taken from the stack budget.
    static constexpr uint32_t Pattern = 0x5a17c0deu;
    // The guarded words (the application: [_ebss, _ebss + Words)); tests pass their own storage.
    static void arm(volatile uint32_t *words);
    static void arm(); // The application on the MCU only: just above _ebss.
    // Words still holding the pattern, from the lowest one up to the first broken one.
    static size_t intact();
    // A pass of the main loop: a broken guard never returns (Watchdog::stackOverflow).
    static void check();
};
} // namespace openosc
