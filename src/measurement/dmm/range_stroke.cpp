#include "openosc/measurement/dmm/range_stroke.hpp"

namespace openosc::dmm {
bool RangeStroke::start(End end, uint32_t now) {
    if (active()) return false;
    end_ = end; physical_ = end == End::Open; closing_ = false;
    enter(State::PressDue, now);
    return true;
}
bool RangeStroke::finish(End end, uint32_t now) {
    if (!open() || end == End::Open) return false;
    end_ = end; finishedAt_ = now;
    return true;
}
void RangeStroke::close(uint32_t now) {
    if (state_ == State::Idle) return;
    if (state_ == State::PressDue) { state_ = State::Idle; return; } // Nothing went: nothing owed.
    closing_ = true;
    if (state_ == State::EndSending) return;                         // Its 21 or 22 is already on its way.
    end_ = End::Release; physical_ = true; finishedAt_ = now;        // The 20 went (or is going): its 21.
}
uint32_t RangeStroke::endAt() const {
    if (!physical_) return downAt_ + (end_ == End::Hold ? HoldGapMs : ReleaseGapMs);
    const uint32_t earliest = downAt_ + MinGapMs; // A release before the 20 left: right after it.
    return int32_t(finishedAt_ - earliest) > 0 ? finishedAt_ : earliest;
}
RangeStroke::Event RangeStroke::service(DataStatus tx, uint32_t now) {
    if (state_ == State::PressSending || state_ == State::EndSending) {
        if (tx == DataStatus::Busy) {
            if (now - since_ < StepLimitMs) return Event::None;
            state_ = State::Idle; return Event::Failed;                // A transmission that never ends.
        }
        if (tx != DataStatus::Ok) { state_ = State::Idle; return Event::Failed; }
        if (state_ == State::EndSending) { state_ = State::Idle; return Event::Done; }
        enter(State::Down, now); downAt_ = now;
    }
    if (state_ == State::PressDue && now - since_ >= StepLimitMs) { state_ = State::Idle; return Event::Failed; }
    if (state_ == State::Down && end_ == End::Open && now - downAt_ >= OpenLimitMs) close(now);
    if (state_ == State::Down && end_ != End::Open && int32_t(now - endAt()) >= int32_t(StepLimitMs)) {
        state_ = State::Idle; return Event::Failed;                    // Its end due and never sent.
    }
    return Event::None;
}
bool RangeStroke::due(uint32_t now, CommandGroup &command) const {
    if (state_ == State::PressDue) { command = CommandGroup::NextRange; return true; }
    if (state_ != State::Down || end_ == End::Open || int32_t(now - endAt()) < 0) return false;
    command = end_ == End::Hold ? CommandGroup::AutoRange : CommandGroup::RangeRelease;
    return true;
}
RangeStroke::Event RangeStroke::accepted(bool ok, uint32_t now) {
    if (state_ != State::PressDue && state_ != State::Down) return Event::None;
    if (!ok) { state_ = State::Idle; return Event::Failed; }
    if (state_ == State::PressDue) { enter(State::PressSending, now); return Event::PressSent; }
    enter(State::EndSending, now); return Event::EndSent;
}
} // namespace openosc::dmm
