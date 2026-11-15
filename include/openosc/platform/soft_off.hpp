#pragma once
#include <cstdint>

namespace openosc {
// The power key after a power off whose power did not go away (docs/POWER.md, Q-08): the latch is
// released, so on battery the instrument is off; on external power the MCU may still run, dark. The
// key must first be seen released for ReleaseMs (the hold that powered off, its bounces), then held
// for HoldMs: then the instrument restarts. Shorter presses do nothing. Pure: one call per ms.
class SoftOff {
public:
    static constexpr uint32_t ReleaseMs = 100, HoldMs = 1000;
    // One ms with the key `held`; true once the restart is due (and from then on).
    bool tick(bool held) {
        if (due_) return true;
        if (!armed_) {
            released_ = held ? 0 : released_ + 1;
            armed_ = released_ >= ReleaseMs;
            return false;
        }
        held_ = held ? held_ + 1 : 0;
        due_ = held_ >= HoldMs;
        return due_;
    }
    bool armed() const { return armed_; }

private:
    uint32_t released_ = 0, held_ = 0;
    bool armed_ = false, due_ = false;
};
} // namespace openosc
