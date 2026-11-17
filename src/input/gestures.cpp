#include "openosc/input/gestures.hpp"
#include "openosc/platform/input.hpp"

namespace openosc {
static_assert(KeyGestures::PressKeys == Keypad::AutoRange, "press: AUTO/RANGE only");
static_assert(Keypad::Relative < (1U << 16) && sizeof(KeyEvents) == 14, "every key in KeyEvents::Mask, no padding");
KeyEvents KeyGestures::poll(uint32_t keys, uint32_t now) {
    KeyEvents events;
    for (unsigned i = 0; i < sizeof buttons_ / sizeof *buttons_; ++i) {
        auto& b = buttons_[i];
        const KeyEvents::Mask bit = KeyEvents::Mask(1U << i);
        const bool arrow = bit & (Keypad::Up | Keypad::Down | Keypad::Left | Keypad::Right);
        const uint32_t holdMs = bit & (Keypad::Relative | Keypad::AutoRange) ? LongHoldMs : HoldMs;
        const bool pressed = keys & bit;
        if (!initialized_) {
            b.candidate = b.down = pressed;
            b.armed = !pressed;
            b.changedAt = now;
            continue;
        }
        if (b.candidate != pressed) { b.candidate = pressed; b.changedAt = now; }
        if (b.down != b.candidate && now - b.changedAt >= DebounceMs) {
            b.down = b.candidate;
            if (b.down) {
                b.pressedAt = now;
                b.rawAt = b.changedAt;
                b.qualifyAt = b.changedAt + PressQualifyMs; // From the raw edge, as the factory's 50 ms.
                b.holdAt = b.changedAt + PressHoldMs;
                b.qualified = false;
                b.repeatAt = now + RepeatDelayMs;
                b.emitted = false;
                if (arrow && b.armed) {
                    events.shortPress |= bit;
                    b.emitted = true; // Arrows act on press, never again on release.
                }
            }
            else {
                // A qualified press ends: before the window its release, from it on a suppressed one.
                if ((bit & PressKeys) && b.qualified) {
                    if (b.changedAt - b.rawAt < PressWindowMs) events.release |= bit;
                    else events.releaseLate |= bit;
                    b.qualified = false;
                }
                if (b.armed && !b.emitted) {
                    const uint32_t held = b.changedAt - b.pressedAt; // To the raw release edge.
                    if (held >= holdMs) events.longPress |= bit;
                    else if (!b.shortless && !(bit == Keypad::Relative && held >= PowerCommitMs)) events.shortPress |= bit;
                }
                b.armed = true; b.shortless = false;
            }
        }
        if (arrow && b.down && pressed && b.armed && int32_t(now - b.repeatAt) >= 0) {
            events.repeat |= bit;
            b.repeatAt = now + RepeatPeriodMs; // No burst after a delayed poll.
        }
        if ((bit & PressKeys) && b.down && pressed && b.armed && !b.shortless && !b.qualified && int32_t(now - b.qualifyAt) >= 0) {
            events.press |= bit;
            b.qualified = true;
        }
        if ((bit & PressKeys) && b.down && pressed && b.qualified && int32_t(now - b.holdAt) >= 0) {
            events.holdRepeat |= bit;
            b.holdAt = now + PressHoldMs; // No burst after a delayed poll.
        }
        if (!arrow && b.down && pressed && b.armed && !b.emitted && now - b.pressedAt >= holdMs) {
            events.longPress |= bit;
            b.emitted = true;
        }
    }
    initialized_ = true;
    return events;
}
bool KeyGestures::holding(uint32_t bit, uint32_t now, uint32_t &elapsed) const {
    const unsigned i = index(bit);
    if (i >= sizeof buttons_ / sizeof *buttons_) return false;
    const auto &b = buttons_[i];
    if (!initialized_ || !b.down || !b.armed || b.emitted) return false;
    elapsed = now - b.pressedAt;
    return true;
}
void KeyGestures::holdOnly(uint32_t bit, uint32_t now) {
    const unsigned i = index(bit);
    if (i >= sizeof buttons_ / sizeof *buttons_) return;
    auto &b = buttons_[i];
    if (!b.down) return;
    b.armed = true; b.emitted = false; b.pressedAt = now; b.shortless = true;
}
}
