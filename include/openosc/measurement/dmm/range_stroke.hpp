#pragma once
#include "openosc/data/status.hpp"
#include "openosc/measurement/dmm/session.hpp"

namespace openosc::dmm {
// The factory's AUTO/RANGE key (KEY5) on the wire, the only sender of 20/21/22 (703S slot1 08044274 /
// 706S 08044BE4: 20 20; slot0 080442B8 / 08044C28: 21 21; slot2 0804423C / 08044BAC: 22 22).
//
// One stroke at a time; while one runs nothing else uses the UART (MeterCommands). Each pair goes only
// once the previous one has left the UART (txStatus Ok): no 21/22 unless its 20 went out. A refused send,
// a failed transmission or a step that waits StepLimitMs (the 20 not sent, a pair still transmitting,
// an end due and not sent) ends the stroke at once (Failed): no retry, nothing further.
//
// Physical (start Open: the key is down; the factory's poll, 703S 080459F4 / 706S 08046390): one held
// state from the 20 to the key's release. release(true) (let go before the factory's long flag) sends
// 21; release(false) (let go later) sends nothing and only ends the stroke (Released); hold() (2 s, and
// every 2 s while held) sends a 22 each time, never a second 20 and never a 21 after a 22. A pair asked
// for before the 20 has left goes MinGapMs after it. No input for QuietLimitMs (press, hold) while down:
// the key's events are lost, close(). Synthetic (start Release/Hold: SCPI, menu, RangeSelector, restore):
// the end at ReleaseGapMs (our chosen synthetic short press, not a factory constant) or HoldGapMs after
// the 20.
//
// close() is the controlled cancel (leaving the DMM, shutdown, a context that took the key, lost input):
// before the 20 nothing is sent; up to ShortMs after the stroke began and before any 22, a 21 (as the
// factory's release before its long flag); later nothing (as the factory's suppressed release). A pair
// already on its way is only waited for. A closing stroke's command is cancelled by its owner: it is
// never confirmed.
class RangeStroke {
public:
    enum class End : uint8_t { Open, Release, Hold };
    // What a step did, for the command that owns the stroke: PressSent (20 accepted: its origin),
    // EndSent (the first 21/22 accepted: its effect counts from here), Released (a physical stroke
    // ended with no pair after its 20: its effect counts from here), Done (the stroke over, its last
    // pair transmitted), Failed.
    enum class Event : uint8_t { None, PressSent, EndSent, Released, Done, Failed };
    // ShortMs: the factory's 500 ms long flag, counted from the qualified press (50 ms after the raw
    // edge, where a physical stroke starts). QuietLimitMs: a key held down gives an event at least every
    // 2 s (KeyGestures::PressHoldMs); a stroke longer without one has lost its input.
    static constexpr uint32_t MinGapMs = 20, ReleaseGapMs = 90, HoldGapMs = 1950, ShortMs = 450,
                              QuietLimitMs = 2600, StepLimitMs = 200;
    bool start(End end, uint32_t now);           // False while a stroke runs.
    bool release(bool sendRelease, uint32_t now); // The physical key let go: 21 or nothing; false otherwise.
    bool hold(uint32_t now);                     // The physical key held to a 22 (each one); false otherwise.
    Event service(DataStatus tx, uint32_t now);   // The UART's transmit state, the step limits.
    bool due(uint32_t now, CommandGroup &command) const; // The pair to send now (the UART is not busy).
    Event accepted(bool ok, uint32_t now);      // The result of sending what due() gave.
    void close(uint32_t now);
    void abandon() { state_ = State::Idle; }     // The UART stopped or failed: nothing more can be sent.
    bool active() const { return state_ != State::Idle; }
    bool open() const { return active() && physical_ && !released_ && !closing_; } // The key is still down.
    bool closing() const { return active() && closing_; }

private:
    enum class State : uint8_t { Idle, PressDue, PressSending, Down, EndSending };
    enum class Pending : uint8_t { None, Release, Hold };
    uint32_t endAt() const;
    void enter(State state, uint32_t now) { state_ = state; since_ = now; }
    State state_ = State::Idle;
    Pending pending_ = Pending::None;
    bool physical_ = false, closing_ = false, released_ = false, held_ = false, endSent_ = false;
    uint32_t since_ = 0, startedAt_ = 0, downAt_ = 0, askedAt_ = 0, inputAt_ = 0;
};
} // namespace openosc::dmm
