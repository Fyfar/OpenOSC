#pragma once
#include <cstdint>
#include "openosc/platform/watchdog_policy.hpp"

namespace openosc {
// The independent watchdog IWDG1 (LSI), owned by each of our images from its start (docs/WATCHDOG.md).
// The factory boot already runs it (prescaler 16, reload 0xFFF); start() takes it over with our timeout
// whether it runs or not. Feeding follows WatchdogPolicy: only thread mode with interrupts enabled
// feeds, so an interrupt that keeps running (SysTick, USB) never vouches for a stuck main loop.
class Watchdog {
public:
    // Prescaler code 3 (/32) and reload 4095: 4096 counts, 4.1 s at the nominal 32 kHz LSI.
    static constexpr uint32_t PrescalerCode = 3, Reload = 0xfff, NominalTimeoutMs = 4096;
    // SR polls for the prescaler, reload and window updates (a few LSI periods: about 0.2 ms each).
    static constexpr uint32_t UpdatePolls = 1000000;
    // boot_diagnostics.watchdog bits.
    enum Status : uint32_t { Configured = 1, UpdateTimeout = 2, Halted = 4, Stalled = 8 };
    // ResetRecord detail of an InitFailure when start() could not confirm the new settings.
    static constexpr uint32_t StartFailure = 0x57440001;

    // Takes IWDG1 over with our settings; if the update cannot be confirmed (SR), records why and
    // resets at once instead of running on unknown settings (fail closed).
    static void start();
    // Servicing in a wait (Board::service): feeds while the policy allows, the IWDG only on a new ms
    // tick (the inherited WWDG at every call, inside its window). `caller` is recorded
    // once if feeding stops (a hint to the stuck wait, not an exact location).
    static void service(uint32_t now, uintptr_t caller);
    static void heartbeat();          // A pass of the main loop.
    static void progress();           // A step of a long operation inside one pass.
    static void expect(uint32_t ms);  // An operation of up to `ms` (bounded) in this pass.
    // A failure the image cannot continue from: recorded, never fed again, reset by the watchdog.
    [[noreturn]] static void fatal(uint32_t error);
    // The application stack reached its guard (StackGuard): recorded as StackOverflow (detail: the
    // guard words that survived), never fed again, reset by the watchdog like fatal().
    [[noreturn]] static void stackOverflow(uint32_t survivedWords);
    // Soft off only (PowerControl::powerOff, docs/POWER.md): feeds the IWDG and an inherited WWDG
    // whatever the context, interrupts masked and the ms tick stopped included. Nothing else may.
    static void terminalFeed();
    static uint32_t status();
    static const WatchdogPolicy &policy();
};

// The previous run's reset flags (RCC_RSR) and ResetRecord into boot_diagnostics. Recovery only
// reads; the application consumes (clears RSR and the record) so the next boot shows a new cause.
class ResetDiagnostics {
public:
    static void capture(bool consume);
};
} // namespace openosc
