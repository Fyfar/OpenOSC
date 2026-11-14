#pragma once
#include "openosc/calibration/zero_profile.hpp"
#include "openosc/acquisition/request.hpp"

namespace openosc::calibration {
// The measurement both zero calibrations share: trial frames of a grounded input at one range and
// node, after the analog path settled, gathered into a histogram per channel and judged around
// its median (a real offset or signal moves the whole distribution, a bus glitch is one sample far
// from it). The expected zero code is the frame's own metadata, which must stay the same.
class ZeroSampler {
public:
    static constexpr uint32_t SettleMs=scope::AnalogFrontend::SettleMs,OutlierDivisor=2048;
    enum class Take { None,Taken,Uncalibrated };
    struct Statistics { double mean=0,variance=0; int minimum=0,maximum=0; uint32_t outliers=0; };
    // Both channels on, DC, at `range` and node `point`, 1 ms/div, no trigger, probe 1:1.
    static ScopeRequest trial(const ScopeRequest &original,unsigned range,unsigned point);
    // A new measurement of `total` samples per channel at `range`, the analog path changed at `now`.
    void reset(unsigned range,uint32_t total,uint32_t now);
    bool settled(uint32_t now) const { return uint32_t(now-appliedAt_)>=SettleMs; }
    // The samples (up to the total) of a new frame taken after settling at this range.
    Take take(const scope::FrameView &frame,uint16_t factoryCrc);
    uint32_t samples() const { return samples_; }
    bool complete() const { return samples_>=total_; }
    double expected(unsigned channel) const { return expected_[channel]; }
    Statistics statistics(unsigned channel) const;
    // Outliers at most one in OutlierDivisor, |mean - expected| within `deviation` codes (the
    // procedure's limit), variance at most 9 codes^2, never at the ADC's ends.
    bool quality(double deviation) const;
    // |mean of the first half - mean of the second half| of a channel's samples: the input still
    // moving (an offset PWM not yet settled) where it should stand.
    double drift(unsigned channel) const;
private:
    bool matches(const scope::FrameView &frame) const;
    unsigned range_=0;
    uint32_t samples_=0,total_=0,appliedAt_=0,sequence_=0;
    bool hasSequence_=false;
    uint32_t histogram_[2][256]{};
    double expected_[2]{},halves_[2][2]{};
    uint32_t halfCounts_[2]{};
};
} // namespace openosc::calibration
