#pragma once
#include "openosc/measurement/dmm/function.hpp"
#include "openosc/measurement/dmm/session.hpp"

namespace openosc::dmm {
// One controller key (AUTO/RANGE, HOLD, REL) pressed once, confirmed by a fresh packet. A range command
// (Auto, Manual, NextRange) is one RangeStroke (DataAccess): judged against the state when its 20 was
// accepted (pressed) and from its end (sent: 21 or 22, or a physical release past the factory's window
// with no pair), confirmed only after that end and only in that function: Auto by AUTO after the 22;
// Manual or NextRange from Auto by Manual; from Manual, a newly recognised range index of the same
// function (as RangeSelector's steps), in any packet newer than the 20. A transitional unknown function or another function never
// confirms. Manual is the decoder's (Decoder::range: MANU, or a reading without AUTO in a ranged
// function). Entering Manual that live packets of the same function (one first observed
// RangeSelector::ManualMs after the end) still show in AUTO: Unsupported. Anything else
// unconfirmed, a silent controller included: Timeout, DeadlineMs after the end (or the request, if the
// stroke never started). One key, never a second. HOLD release and REL off: one command, sent().
class KeyCommand {
public:
    static constexpr uint32_t DeadlineMs = 3000;
    enum class Action { Auto, Manual, NextRange, ReleaseHold, RelativeOff };
    bool request(Action action, const Result &latest, uint32_t now, uint64_t minimumTimestampUs = 0);
    bool poll(const Result &latest, uint32_t now, CommandGroup &command);
    void sent(uint32_t sequence, uint32_t now) {
        sent_ = true; sequence_ = seen_ = sequence; sentAt_ = seenAt_ = now; earliest_ = now + 150; deadline_ = now + DeadlineMs;
    }
    // A range stroke's 20 accepted: the origin, and no further command from poll. Its end (sent: its
    // 21 or 22, or a physical release with no pair) is due within StrokeMs (RangeStroke: the first hold
    // or the quiet limit and their step limits), else Timeout.
    static constexpr uint32_t StrokeMs = 3000;
    void pressed(const Result &latest, uint32_t now);
    // The physical AUTO/RANGE held to its 22 (KeyGestures long): the stroke opened as NextRange is Auto.
    void hold() { if (busy() && !sent_ && action_ == Action::NextRange) action_ = Action::Auto; }
    void fail() { status_ = DataStatus::HardwareError; }
    void cancel() { status_ = DataStatus::NoData; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
private:
    bool matches(const Result &result) const;
    bool confirms(const Result &result) const; // After the send: matches, judged against the origin for a range press.
    bool rangePress() const { return action_ == Action::Manual || action_ == Action::NextRange; }
    Action action_ = Action::Auto;
    DataStatus status_ = DataStatus::NoData;
    uint32_t sequence_ = 0, deadline_ = 0, earliest_ = 0, sentAt_ = 0;
    uint32_t seen_ = 0, seenAt_ = 0; // The latest sequence observed and when (live packets vs silence).
    Function function_ = Function::Unknown; // The origin of a range command, taken when its 20 is accepted.
    unsigned index_ = 0;
    uint32_t pressSequence_ = 0; // The latest packet when the 20 was accepted.
    bool originManual_ = false, originIndexed_ = false;
    bool sent_ = false, pressed_ = false;
    uint64_t minimumTimestampUs_ = 0;
};
} // namespace openosc::dmm
