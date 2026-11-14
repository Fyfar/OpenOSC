#include "openosc/calibration/zero_operation.hpp"
#include <algorithm>

namespace openosc::calibration {
bool ZeroOperation::begin(uint32_t now,ZeroProcedure &procedure) {
    if (busy()) return false;
    const auto ready=instrument_.zeroAvailable();
    if (ready!=DataStatus::Ok || !procedure.available()) {
        result_=ready==DataStatus::Uncalibrated ? Result::Uncalibrated : !procedure.available() ? Result::IoError : Result::Busy;
        return false;
    }
    if (procedure.shadowed()) { result_=Result::Shadowed; return false; } // Nothing measured, nothing saved.
    // HOLD: the record on show is kept aside first; if it cannot be, nothing is changed at all.
    const bool held=instrument_.zeroHeld();
    const ScopeRequest original=instrument_.zeroRequest();
    if (held && !instrument_.zeroRetain()) { result_=Result::Busy; return false; }
    if (!instrument_.zeroStopGenerator()) {
        if (held) instrument_.zeroRestoreHeld(original);
        result_=Result::HardwareError; return false;
    }
    procedure_=&procedure;
    procedure_->begin(original,instrument_.zeroFactoryCrc(),now);
    retained_=held; holdAfter_=held; waitForFrame_=true; configured_=false; instrument_.zeroHold(false);
    phase_=Phase::Applying; result_=Result::Busy; deadline_=now+12000;
    return true;
}
// Whatever ended the procedure, no trial of it stays in force when the original request returns.
void ZeroOperation::restore(uint32_t now) {
    procedure_->cancel(); procedure_->finish(); phase_=Phase::Restoring; deadline_=now+12000;
}
void ZeroOperation::cancel(uint32_t now) {
    if (phase_==Phase::Applying || phase_==Phase::Collecting) {
        result_=Result::Cancelled;
        restore(now);
    } else if (phase_==Phase::Restoring) {
        holdAfter_=false; waitForFrame_=false;
    } else if (phase_==Phase::Resuming) {
        // The requested hardware is already restored. The user may resume RUN
        // without waiting up to200s for a new HOLD frame. No trial frame survives
        // the successful forced reconfiguration.
        instrument_.zeroHold(false); phase_=Phase::Idle;
    }
}
void ZeroOperation::holdAfter(bool hold,uint32_t now) {
    holdAfter_=hold;
    if (phase_==Phase::Applying || phase_==Phase::Collecting) cancel(now);
}
const ScopeRequest *ZeroOperation::abandon() {
    if (!busy()) return nullptr;
    procedure_->cancel(); procedure_->finish(); phase_=Phase::Idle; result_=Result::Cancelled;
    if (retained_) { instrument_.zeroReleaseRetained(); retained_=false; }
    return &procedure_->original();
}
void ZeroOperation::service(uint32_t now) {
    if (!busy()) return;
    if (int32_t(now-deadline_)>=0) {
        if (phase_==Phase::Applying || phase_==Phase::Collecting) {
            result_=Result::Timeout; restore(now);
        } else {
            instrument_.zeroRestoreFailed(procedure_->original());
            if (retained_) { instrument_.zeroReleaseRetained(); retained_=false; }
            phase_=Phase::Idle; result_=Result::RestoreFailed;
        }
        return;
    }
    if (phase_==Phase::Applying) {
        if (!configured_ && !instrument_.zeroConfigure(procedure_->trial())) return;
        configured_=true;
        // The settle time runs from the hardware's change, not from the configure: a deferred
        // capture start applies the front end later (every frame must start after it settled).
        uint32_t at=0;
        if (!instrument_.zeroAppliedAt(at)) return;
        configured_=false;
        procedure_->applied(at); phase_=Phase::Collecting;
        deadline_=now+6000;
    } else if (phase_==Phase::Collecting) collecting(now);
    else if (phase_==Phase::Restoring) {
        if (retained_ && holdAfter_) { // The original HOLD record, as it was.
            if (!instrument_.zeroRestoreHeld(procedure_->original())) return;
            retained_=false; phase_=Phase::Idle;
            return;
        }
        if (retained_) { instrument_.zeroReleaseRetained(); retained_=false; } // RUN chosen instead.
        instrument_.zeroHold(false);
        if (!instrument_.zeroConfigure(procedure_->original())) return;
        if (!waitForFrame_) { phase_=Phase::Idle; return; }
        resumeSequence_=instrument_.zeroSequence(); phase_=Phase::Resuming;
        const uint32_t sweep=uint32_t(procedure_->original().timebaseNs/100000);
        deadline_=now+std::max<uint32_t>(12000,sweep+2000);
    } else if (phase_==Phase::Resuming) {
        scope::FrameLease frame;
        if (!instrument_.zeroAcquire(frame) || int32_t(frame.view().metadata().sequence-resumeSequence_)<0) return;
        instrument_.zeroHold(holdAfter_);
        phase_=Phase::Idle;
    }
}
void ZeroOperation::collecting(uint32_t now) {
    using State=ZeroProcedure::State;
    procedure_->tick(now);
    bool advanced=false;
    {
        scope::FrameLease frame;
        if (procedure_->active() && instrument_.zeroAcquire(frame)) advanced=procedure_->observe(frame.view(),now);
    } // Release the old frame before changing range or writing the profile.
    if (procedure_->waiting()) deadline_=now+6000; // The user acts; the procedure times the wait.
    switch (procedure_->state()) {
    case State::Complete:
        // Saved first, then in force (the hardware zero's overlay); a failed save changes nothing.
        result_=procedure_->commit() ? Result::Complete : Result::IoError;
        restore(now);
        break;
    case State::InvalidInput: result_=Result::InvalidInput; restore(now); break;
    case State::Uncalibrated: result_=Result::Uncalibrated; restore(now); break;
    case State::Timeout: result_=Result::Timeout; restore(now); break;
    default:
        if (advanced) { phase_=Phase::Applying; configured_=false; deadline_=now+12000; }
        break;
    }
}
} // namespace openosc::calibration
