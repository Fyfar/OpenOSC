#pragma once
#include "openosc/data/status.hpp"
#include "openosc/measurement/dmm/session.hpp"

namespace openosc::dmm {
// The factory's AUTO/RANGE key (KEY5) on the wire, the only sender of 20/21/22: 20 20 once the press
// has qualified (slot1: 703S 08044274, 706S 08044BE4), then either 21 21 on a short release (slot0:
// 080442B8 / 08044C28) or 22 22 on the hold (slot2: 0804423C / 08044BAC), never 21 after 22.
//
// One stroke at a time; while one runs nothing else uses the UART (MeterCommands). Each pair goes only
// once the previous one has left the UART (txStatus Ok): no 21/22 unless its 20 went out. A refused send,
// a failed transmission or a step that waits StepLimitMs (the 20 not sent, a pair still transmitting,
// an end due and not sent) ends the stroke at once (Failed): no retry, nothing further.
//
// Physical (Open): the end comes from the key (finish), at least MinGapMs after the 20. Synthetic (SCPI,
// menu, RangeSelector, restore): the end is given at start, ReleaseGapMs (our chosen synthetic short
// press, not a factory constant: there the 21 follows the physical release) or HoldGapMs (20 at 50 ms, 22 at the 2 s hold) after
// the 20. close() is the controlled cancel (leaving the DMM, shutdown, a context that took the key, an
// open stroke past OpenLimitMs): before the 20 nothing is sent; after it, a stroke whose 22 has not gone
// ends with a 21 MinGapMs after the 20 (a 22 or 21 already sent is only waited for). A closing stroke's
// command is cancelled by its owner: it is never confirmed.
class RangeStroke {
public:
    enum class End : uint8_t { Open, Release, Hold };
    // What a step did, for the command that owns the stroke: PressSent (20 accepted: its origin),
    // EndSent (21/22 accepted: its effect counts from here), Done (the end transmitted), Failed.
    enum class Event : uint8_t { None, PressSent, EndSent, Done, Failed };
    static constexpr uint32_t MinGapMs = 20, ReleaseGapMs = 90, HoldGapMs = 1950, OpenLimitMs = 2500, StepLimitMs = 200;
    bool start(End end, uint32_t now);         // False while a stroke runs.
    bool finish(End end, uint32_t now);        // An open stroke's end (Release or Hold); false otherwise.
    Event service(DataStatus tx, uint32_t now); // The UART's transmit state, the step limits.
    bool due(uint32_t now, CommandGroup &command) const; // The pair to send now (the UART is not busy).
    Event accepted(bool ok, uint32_t now);    // The result of sending what due() gave.
    void close(uint32_t now);
    void abandon() { state_ = State::Idle; }   // The UART stopped or failed: nothing more can be sent.
    bool active() const { return state_ != State::Idle; }
    bool open() const { return active() && end_ == End::Open; }
    bool closing() const { return active() && closing_; }
private:
    enum class State : uint8_t { Idle, PressDue, PressSending, Down, EndSending };
    uint32_t endAt() const;
    void enter(State state, uint32_t now) { state_ = state; since_ = now; }
    State state_ = State::Idle;
    End end_ = End::Open;
    bool physical_ = false, closing_ = false;
    uint32_t since_ = 0, downAt_ = 0, finishedAt_ = 0;
};
} // namespace openosc::dmm
