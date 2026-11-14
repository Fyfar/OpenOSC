#pragma once
#include "openosc/measurement/dmm/session.hpp"

namespace openosc::dmm {
class KeyCommand {
public:
    enum class Action { Auto, Manual, NextRange, ReleaseHold, RelativeOff };
    bool request(Action action, const Result &latest, uint32_t now, uint64_t minimumTimestampUs = 0);
    bool poll(const Result &latest, uint32_t now, CommandGroup &command);
    void sent(uint32_t sequence, uint32_t now) { sent_ = true; sequence_ = sequence; earliest_ = now + 150; }
    void fail() { status_ = DataStatus::HardwareError; }
    void cancel() { status_ = DataStatus::NoData; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
private:
    bool matches(const Result &result) const;
    Action action_ = Action::Auto;
    DataStatus status_ = DataStatus::NoData;
    uint32_t sequence_ = 0, deadline_ = 0, earliest_ = 0;
    bool sent_ = false;
    uint64_t minimumTimestampUs_ = 0;
};
} // namespace openosc::dmm
