#pragma once
#include <cstdint>

namespace openosc {
struct KeyEvents {
    uint32_t shortPress = 0;
    uint32_t longPress = 0;
    uint32_t repeat = 0;
};
class KeyGestures {
public:
    static constexpr uint32_t DebounceMs = 25, HoldMs = 1000;
    // REL/Power and AUTO/RANGE hold as the factory's KEY4/KEY5: four 500 ms intervals (703S 080459F4).
    static constexpr uint32_t LongHoldMs = 2000;
    // REL/Power: from the first interval (500 ms after the debounced press) the power-off progress
    // shows, and a release from then on cancels it with no REL (as the factory's poll); a release
    // before it is REL. AUTO/RANGE keeps its short press below LongHoldMs.
    static constexpr uint32_t PowerCommitMs = 500;
    static constexpr uint32_t RepeatDelayMs = 400, RepeatPeriodMs = 80;
    KeyEvents poll(uint32_t keys, uint32_t now);
    // A non-arrow key held towards its long press (pressed after start, not yet emitted): the time
    // since its debounced press. False otherwise (released, already long, held since before start).
    bool holding(uint32_t bit, uint32_t now, uint32_t &elapsed) const;
    // After a wake: `bit`, if held, still counts towards its long press from `now`, but its release
    // before that gives no short press (the wake took this stroke).
    void holdOnly(uint32_t bit, uint32_t now);
private:
    struct Button {
        uint32_t changedAt = 0, pressedAt = 0, repeatAt = 0;
        bool candidate = false, down = false, armed = false, emitted = false, shortless = false;
    } buttons_[13];
    static unsigned index(uint32_t bit) { unsigned i = 0; while (i < 31 && !(bit & (1U << i))) ++i; return i; }
    bool initialized_ = false;
};
}
