#include "openosc/measurement/dmm/key_command.hpp"

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
bool KeyCommand::request(Action action, const Result &latest, uint32_t now, uint64_t minimumTimestampUs) {
    if (busy()) return action_ == action;
    action_ = action; sent_ = false; sequence_ = latest.sequence; deadline_ = now + 3000;
    minimumTimestampUs_ = minimumTimestampUs;
    status_ = latest.sequence && latest.timestampUs >= minimumTimestampUs_ && action != Action::NextRange && matches(latest) ? DataStatus::Ok : DataStatus::Busy;
    return true;
}
bool KeyCommand::poll(const Result &latest, uint32_t now, CommandGroup &command) {
    if (!busy()) return false;
    if (latest.timestampUs >= minimumTimestampUs_ && ((sent_ && latest.sequence != sequence_ && int32_t(now - earliest_) >= 0) ||
        (!sent_ && action_ != Action::NextRange && latest.sequence)) && matches(latest)) {
        status_ = DataStatus::Ok; return false;
    }
    if (int32_t(now - deadline_) >= 0) { status_ = DataStatus::Timeout; return false; }
    if (latest.timestampUs < minimumTimestampUs_ || !latest.sequence) return false;
    if (sent_) return false;
    command = action_ == Action::Auto ? CommandGroup::AutoRange : action_ == Action::ReleaseHold ? CommandGroup::PeerHold :
              action_ == Action::RelativeOff ? CommandGroup::PeerRelative : CommandGroup::NextRange;
    return true;
}
} // namespace openosc::dmm
