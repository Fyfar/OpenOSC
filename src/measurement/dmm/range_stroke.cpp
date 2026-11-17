#include "openosc/measurement/dmm/range_stroke.hpp"

namespace openosc::dmm {
bool RangeStroke::start(End end, uint32_t now) {
    if (active()) return false;
    physical_ = end == End::Open; released_ = !physical_; closing_ = held_ = endSent_ = false;
    pending_ = end == End::Release ? Pending::Release : end == End::Hold ? Pending::Hold : Pending::None;
    startedAt_ = inputAt_ = askedAt_ = now;
    enter(State::PressDue, now);
    return true;
}
bool RangeStroke::release(bool sendRelease, uint32_t now) {
    if (!open()) return false;
    released_ = true;
    // Never a 21 after a 22 (sent or still asked for): the factory's long flag is set by then.
    if (sendRelease && !held_ && pending_ == Pending::None) { pending_ = Pending::Release; askedAt_ = now; }
    return true;
}
bool RangeStroke::hold(uint32_t now) {
    if (!open()) return false;
    inputAt_ = now; held_ = true;
    if (pending_ != Pending::Hold) { pending_ = Pending::Hold; askedAt_ = now; }
    return true;
}
void RangeStroke::close(uint32_t now) {
    if (state_ == State::Idle) return;
    if (state_ == State::PressDue) { state_ = State::Idle; return; } // Nothing went: nothing owed.
    closing_ = released_ = true;
    // A pair already on its way, or one that went: only waited for; a further 22 is never sent.
    if (state_ == State::EndSending || endSent_) { pending_ = Pending::None; return; }
    if (pending_ == Pending::Release) return; // The stroke's own 21 still goes.
    const bool early = !held_ && now - startedAt_ < ShortMs;
    pending_ = early ? Pending::Release : Pending::None;
    askedAt_ = now;
}
uint32_t RangeStroke::endAt() const {
    if (!physical_ && !closing_) return downAt_ + (pending_ == Pending::Hold ? HoldGapMs : ReleaseGapMs);
    const uint32_t earliest = downAt_ + MinGapMs; // Asked for before the 20 left: right after it.
    return int32_t(askedAt_ - earliest) > 0 ? askedAt_ : earliest;
}
RangeStroke::Event RangeStroke::service(DataStatus tx, uint32_t now) {
    if (state_ == State::PressSending || state_ == State::EndSending) {
        if (tx == DataStatus::Busy) {
            if (now - since_ < StepLimitMs) return Event::None;
            state_ = State::Idle; return Event::Failed;                // A transmission that never ends.
        }
        if (tx != DataStatus::Ok) { state_ = State::Idle; return Event::Failed; }
        if (state_ == State::PressSending) { enter(State::Down, now); downAt_ = now; }
        else if (released_ && pending_ == Pending::None) { state_ = State::Idle; return Event::Done; }
        else enter(State::Down, now); // Held after a 22, or a pair still asked for.
    }
    if (state_ == State::PressDue && now - since_ >= StepLimitMs) { state_ = State::Idle; return Event::Failed; }
    if (state_ != State::Down) return Event::None;
    if (pending_ == Pending::None && !released_ && now - inputAt_ >= QuietLimitMs) {
        close(now); // Its input is lost: closing (its owner sees it), the end on the next step.
        return Event::None;
    }
    if (pending_ == Pending::None) {
        if (!released_) return Event::None; // Held: the next hold or the release.
        state_ = State::Idle; return endSent_ ? Event::Done : Event::Released;
    }
    if (int32_t(now - endAt()) >= int32_t(StepLimitMs)) { state_ = State::Idle; return Event::Failed; } // Due and never sent.
    return Event::None;
}
bool RangeStroke::due(uint32_t now, CommandGroup &command) const {
    if (state_ == State::PressDue) { command = CommandGroup::NextRange; return true; }
    if (state_ != State::Down || pending_ == Pending::None || int32_t(now - endAt()) < 0) return false;
    command = pending_ == Pending::Hold ? CommandGroup::AutoRange : CommandGroup::RangeRelease;
    return true;
}
RangeStroke::Event RangeStroke::accepted(bool ok, uint32_t now) {
    if (state_ != State::PressDue && state_ != State::Down) return Event::None;
    if (!ok) { state_ = State::Idle; return Event::Failed; }
    if (state_ == State::PressDue) { enter(State::PressSending, now); return Event::PressSent; }
    enter(State::EndSending, now); pending_ = Pending::None;
    if (endSent_) return Event::None; // A further 22 of a held key: the wire only.
    endSent_ = true; return Event::EndSent;
}
} // namespace openosc::dmm
