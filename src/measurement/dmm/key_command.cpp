#include "openosc/measurement/dmm/key_command.hpp"
#include "openosc/measurement/dmm/ranges.hpp"

namespace openosc::dmm {
bool KeyCommand::matches(const Result &result) const {
    switch (action_) {
    case Action::Auto: return result.range == RangeMode::Auto;
    case Action::Manual:
    case Action::NextRange: return result.range == RangeMode::Manual;
    case Action::ReleaseHold: return !result.packetHold;
    case Action::RelativeOff: return !result.peerRelative;
    }
    return false;
}
bool KeyCommand::confirms(const Result &result) const {
    if (action_ == Action::ReleaseHold || action_ == Action::RelativeOff) return matches(result);
    // Any range command (Auto included): only the function it was sent in confirms it.
    if (Functions::identify(result) != function_ || !matches(result)) return false;
    if (action_ == Action::Auto || !originManual_) return true;
    unsigned index = 0;
    return Ranges::current(result, &index) && (!originIndexed_ || index != index_);
}
void KeyCommand::pressed(const Result &latest, uint32_t now) {
    deadline_ = now + StrokeMs;
    function_ = Functions::identify(latest);
    originManual_ = latest.range == RangeMode::Manual;
    originIndexed_ = originManual_ && Ranges::current(latest, &index_);
    pressSequence_ = latest.sequence;
    pressed_ = true;
}
bool KeyCommand::request(Action action, const Result &latest, uint32_t now, uint64_t minimumTimestampUs) {
    if (busy()) return action_ == action;
    action_ = action; sent_ = pressed_ = false; sequence_ = latest.sequence; deadline_ = now + DeadlineMs;
    minimumTimestampUs_ = minimumTimestampUs;
    status_ = latest.sequence && latest.timestampUs >= minimumTimestampUs_ && action != Action::NextRange && matches(latest) ? DataStatus::Ok : DataStatus::Busy;
    return true;
}
bool KeyCommand::poll(const Result &latest, uint32_t now, CommandGroup &command) {
    if (!busy()) return false;
    if (pressed_ && !sent_) { // Its stroke runs: nothing counts before its end, bounded all the same.
        if (int32_t(now - deadline_) >= 0) status_ = DataStatus::Timeout;
        return false;
    }
    const bool current = latest.timestampUs >= minimumTimestampUs_;
    if (sent_ && current && latest.sequence != seen_) { seen_ = latest.sequence; seenAt_ = now; }
    // A range press (Manual, NextRange) counts any packet newer than its 20: the controller can show
    // Manual between the 20 and the end (a physical key held long, or released past its window). Auto
    // (a 22) counts only packets after the end, as do HOLD release and REL off.
    const uint32_t after = rangePress() && pressed_ ? pressSequence_ : sequence_;
    const bool fresh = sent_ && current && latest.sequence != after;
    if ((fresh && int32_t(now - earliest_) >= 0 && confirms(latest)) ||
        (current && !sent_ && action_ != Action::NextRange && latest.sequence && matches(latest))) {
        status_ = DataStatus::Ok; return false;
    }
    // Entering Manual refused: a packet first seen ManualMs after the send still shows this function in
    // AUTO. An old packet followed by silence, or packets without a range state, are not that (Timeout).
    if (fresh && rangePress() && !originManual_ && Functions::identify(latest) == function_ &&
        latest.range == RangeMode::Auto && seenAt_ - sentAt_ >= RangeSelector::ManualMs) { status_ = DataStatus::Unsupported; return false; }
    if (int32_t(now - deadline_) >= 0) { status_ = DataStatus::Timeout; return false; }
    if (!current || !latest.sequence || sent_) return false;
    command = action_ == Action::Auto ? CommandGroup::AutoRange : action_ == Action::ReleaseHold ? CommandGroup::PeerHold :
              action_ == Action::RelativeOff ? CommandGroup::PeerRelative : CommandGroup::NextRange;
    return true;
}
} // namespace openosc::dmm
