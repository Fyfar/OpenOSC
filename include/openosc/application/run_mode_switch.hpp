#pragma once
#include "openosc/instrument/clock_suspend.hpp"
#include "openosc/settings/run_mode.hpp"
#include "openosc/usb/clock_policy.hpp"
#include <cstdint>

namespace openosc {
// A run-mode (core clock) change without a restart, as one ordered transaction:
//   1. suspend: every PLL1-timed user stops (Busy: nothing changed, the old mode keeps running;
//      Disrupted: the clock is unchanged but a user that stopped did not start again);
//   2. retime: PLL1 to the target profile;
//      on failure the previous profile, then profile 0, so the instrument stays usable;
//   3. resume: the users start again from the bus clocks actually running.
// The host link (USB) is not kept across a real change: once the users are suspended it goes down
// before the first retime; after resume, whatever the outcome, it is told the clock that then runs
// and attaches again only where that clock allows it (usb::ClockPolicy), also when it was down
// before. Unchanged, Refused and Disrupted leave it untouched.
// The result names the profile that runs (`active`), never the requested one when it failed, so
// the caller stores a setting only for Applied and the UI shows the truth.
class RunModeSwitch {
public:
    class Steps {
    public:
        virtual ~Steps() = default;
        virtual ClockSuspend suspend() = 0;
        virtual bool retime(uint32_t mode) = 0;       // True when `mode` now runs.
        virtual bool resume() = 0;
        virtual uint32_t active() const = 0;          // ClockControl::activeMode(): RunModeCount if none.
    };
    class Link {
    public:
        virtual bool attached() const = 0; // Attached, or an attach pending.
        virtual void detach() = 0;         // Stopped now, before the clock changes.
        // After every real change, with the profile that runs (RunModeCount: HSI): attached again later
        // (main loop, after a settle interval) where the link runs on it, else left stopped.
        virtual void settle(uint32_t active) = 0;
    protected:
        ~Link() = default;
    };
    enum class Outcome : uint8_t {
        Applied,      // The target runs, every user resumed.
        Unchanged,    // The target already ran: nothing was stopped.
        Refused,      // A user could not stop (busy): nothing changed.
        Disrupted,    // Not changed: a stop failed and a stopped user did not start again (its status says why).
        Restored,     // The target failed to lock; the previous profile runs again.
        Fallback,     // Target and previous failed; profile 0 runs.
        Failed,       // No profile locked: the core stays on HSI (users resumed at its clocks).
        ResumeFailed, // The clock change held, but a user did not start again (its status says why).
    };
    struct Result { Outcome outcome; uint32_t active; };
    static Result change(Steps &steps, uint32_t target, Link &link);
    static const char *describe(Outcome outcome);
};
// USB Remote runs at 200 MHz (100 MS/s per channel), the only profile with a USB data link: Remote is
// entered only when that profile runs with every user resumed, and changes made for Remote never
// overwrite the user's Standalone profile (the setting), which is restored when Remote ends.
struct RemoteClock {
    static constexpr uint32_t Mode = usb::ClockPolicy::Mode;
    // The target runs with every user resumed.
    static constexpr bool settled(const RunModeSwitch::Result &r) {
        return r.outcome == RunModeSwitch::Outcome::Applied || r.outcome == RunModeSwitch::Outcome::Unchanged;
    }
    static constexpr bool entered(const RunModeSwitch::Result &r) { return settled(r) && r.active == Mode; }
    // A refused Remote change that still moved the clock away from `previous` (the profile that ran
    // before): only then is it changed back. Busy/Disrupted changed nothing; Restored is back already.
    static constexpr bool rollback(const RunModeSwitch::Result &r, uint32_t previous) {
        return !entered(r) && r.outcome != RunModeSwitch::Outcome::Refused &&
               r.outcome != RunModeSwitch::Outcome::Disrupted && r.active != previous && previous < RunModeCount;
    }
    // A host may ask for any profile in Standalone (280/360 then end its own USB link: ClockPolicy);
    // in Remote only for 200 MHz.
    static constexpr bool allowed(uint32_t mode, bool remote) { return !remote || mode == Mode; }
    static constexpr bool persists(bool remote) { return !remote; }
};
} // namespace openosc
