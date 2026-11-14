#pragma once
#include "openosc/settings/run_mode.hpp"
#include <cstdint>

namespace openosc::scope {
// The ZT706S interleave of the factory firmware (zt_fw 706s_main), the parts that decide and correct:
// - InterleavePolicy: when CH1 alone runs through both ADCs (updater 0804A9A8);
// - InterleaveResponse: the display gain of an interleaved CH1 by its frequency (0804D6A4/0804D720);
// - InterleaveRangeBoost: CH1's hardware range one step less sensitive near 46..62 MHz (0804F3F0).
// The capture itself (control bits 8/11, TIM1 CC3P, InterleavedWords, Layout::InterleavedCh1) follows
// from InterleavePolicy in DataAccess, AnalogFrontend, AdcClock and CaptureDma.
struct InterleavePolicy {
    // The factory's interleave needs a non-zero clock setting: here the 280 MHz profile (140 M words/s,
    // 280 MS/s of CH1), the High one. 200 MHz (USB data) and the 360 MHz trial profile never interleave.
    static constexpr uint32_t TimerHz = RunModes[1].coreHz;
    static_assert(RunModes[1].coreHz == 280000000 && RunModes[1].adcDivider == 2, "Interleave runs on the 280 MHz profile");
    // Timebase index < 2 of the factory table: 10 and 25 ns/div.
    static constexpr bool timebase(uint64_t timebaseNs) { return timebaseNs == 10 || timebaseNs == 25; }
    // board: the build's Capabilities::adcInterleave. internal: an AutoSetup or zero trial. busLoaded:
    // the ZT703S DAC stream halves the rate (never on the ZT706S, kept for the rule's completeness).
    static constexpr bool enabled(bool board, uint32_t timerHz, uint64_t timebaseNs, bool ch1, bool ch2, bool rolling,
                                  bool internal, bool busLoaded) {
        return board && timerHz == TimerHz && timebase(timebaseNs) && ch1 && !ch2 && !rolling && !internal && !busLoaded;
    }
};
struct InterleaveResponse {
    static constexpr uint32_t CurveHz = 30000000; // Below it the factory keeps the calibration gains.
    // The factory curve 0804D720, linear through (30, 1.0), (40, 1.0), (46, 1.31), (60, 1.0), (100 MHz,
    // 1.9), each segment from its right node; above 100 MHz the last one goes on. For f >= CurveHz.
    static double curve(uint32_t hz);
    // The factory projector's bounds (08047E28): a gain outside 1..1.99 is drawn with 1.2.
    static constexpr double Fallback = 1.2;
    static double bounded(double gain) { return gain >= 1.0 && gain <= 1.99 ? gain : Fallback; }
    // The absolute display gain of an interleaved CH1 (0 when the factory gain stays: no known
    // frequency, or below CurveHz). It replaces the calibration gain in voltsPerCode only; the ADC
    // zero code stays the factory transfer's at the unchanged offset PWM.
    static double gain(bool known, uint32_t hz) { return known && hz >= CurveHz ? bounded(curve(hz)) : 0; }
};
// 0804F3F0 once per interleaved record: inside (45 800 000, 62 010 000) Hz it is set at once and the
// count of records outside restarts; outside (also no frequency: the factory's estimate is then 0)
// the count rises to 2 and the third consecutive record outside clears it. Leaving the interleave
// clears it at once.
class InterleaveRangeBoost {
public:
    static constexpr uint32_t LowHz = 45800000, HighHz = 62010000; // Exclusive bounds (literals 0804F580/584).
    static constexpr uint8_t Outside = 2;
    static constexpr bool inside(bool known, uint32_t hz) { return known && hz > LowHz && hz < HighHz; }
    void update(bool known, uint32_t hz) {
        if (inside(known, hz)) { outside_ = 0; active_ = true; }
        else if (outside_ < Outside) ++outside_;
        else active_ = false;
    }
    void leave() { active_ = false; outside_ = 0; }
    bool active() const { return active_; }
private:
    bool active_ = false;
    uint8_t outside_ = 0;
};
// The frequency of CH1 one interleaved record shows, as those two rules use it: the record's own
// analysis over a bounded part (MaximumPoints from its start, never a scan of the whole record), and
// the response and range state that follows for the next capture.
class InterleaveTracker {
public:
    static constexpr uint32_t MaximumPoints = 4096;
    // A record captured interleaved; anything else (no interleave) leaves it.
    void observe(bool known, uint32_t hz) {
        boost_.update(known, hz);
        known_ = known; hz_ = known ? hz : 0;
    }
    void leave() { boost_.leave(); known_ = false; hz_ = 0; }
    bool boost() const { return boost_.active(); }
    double gain() const { return InterleaveResponse::gain(known_, hz_); }
    bool known() const { return known_; }
    uint32_t hz() const { return hz_; }
private:
    InterleaveRangeBoost boost_;
    bool known_ = false;
    uint32_t hz_ = 0;
};
} // namespace openosc::scope
