#pragma once
#include <cstdint>

namespace openosc::generator {
// Bounded recovery of the ZT706S module after an exchange failed (NO ACK, a stuck UART): the latest
// requested state (ON with the newest settings, or OFF) goes again after DelayMs, at most Rounds
// times per request. A new request (ON, OFF, changed settings) or an acknowledge gives a fresh
// budget. The factory GEN retries without limit (706S GEN 24071D0C); here the module gets
// Rounds more exchanges, then the failure stays until the user asks again: no endless retries over a
// module that never answers. Modular milliseconds.
class ModuleRecovery {
public:
    static constexpr uint32_t DelayMs = 1000;
    static constexpr unsigned Rounds = 3;
    void reset() { rounds_ = 0; waiting_ = false; }
    void failed(uint32_t now) {
        waiting_ = rounds_ < Rounds;
        if (waiting_) { ++rounds_; at_ = now + DelayMs; }
    }
    // True once when the next round is due.
    bool due(uint32_t now) {
        if (!waiting_ || int32_t(now - at_) < 0) return false;
        waiting_ = false;
        return true;
    }
    bool waiting() const { return waiting_; }
    unsigned rounds() const { return rounds_; }

private:
    uint32_t at_ = 0;
    unsigned rounds_ = 0;
    bool waiting_ = false;
};
} // namespace openosc::generator
