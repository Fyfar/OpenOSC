#include "openosc/application/run_mode_switch.hpp"

namespace openosc {
RunModeSwitch::Result RunModeSwitch::change(Steps &steps, uint32_t target, Link &link) {
    const uint32_t previous = steps.active();
    if (target >= RunModeCount) return {Outcome::Refused, previous};
    if (target == previous) return {Outcome::Unchanged, previous};
    switch (steps.suspend()) {
    case ClockSuspend::Suspended: break;
    case ClockSuspend::Busy: return {Outcome::Refused, steps.active()};
    case ClockSuspend::Disrupted: return {Outcome::Disrupted, steps.active()};
    }
    const bool linked = link.attached();
    if (linked) link.detach();
    if (!steps.prepare(target)) {
        const bool resumed = steps.resume();
        link.settle(steps.active());
        return {resumed ? Outcome::Unguarded : Outcome::Disrupted, steps.active()};
    }
    Outcome outcome = Outcome::Applied;
    if (!steps.retime(target)) {
        if (previous < RunModeCount && steps.retime(previous)) outcome = Outcome::Restored;
        else if (target != 0 && previous != 0 && steps.retime(0)) outcome = Outcome::Fallback; // Each profile tried once.
        else outcome = Outcome::Failed; // HSI: the users resume at its low clocks, the UI says so.
    }
    if (!steps.resume() && outcome == Outcome::Applied) outcome = Outcome::ResumeFailed;
    steps.finished(previous, steps.active());
    link.settle(steps.active());
    return {outcome, steps.active()};
}
const char *RunModeSwitch::describe(Outcome outcome) {
    switch (outcome) {
    case Outcome::Applied: return "Run mode applied";
    case Outcome::Unchanged: return "Run mode already active";
    case Outcome::Refused: return "Run mode busy: finish the current operation";
    case Outcome::Disrupted: return "Run mode not changed; acquisition stopped";
    case Outcome::Restored: return "Clock did not lock; previous mode restored";
    case Outcome::Fallback: return "Clock did not lock; 200 MHz fallback";
    case Outcome::Failed: return "Clock failure: running on HSI, restart";
    case Outcome::ResumeFailed: return "Run mode applied; restart acquisition";
    case Outcome::Unguarded: return "360 MHz not started: boot guard unavailable";
    }
    return "";
}
} // namespace openosc
