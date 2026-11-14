#pragma once
#include "openosc/calibration/pwm_profile.hpp"
#include "openosc/calibration/gain_profile.hpp"
#include "openosc/calibration/interleave_profile.hpp"

namespace openosc::calibration {
// The offset PWM the analog front end applies, one runtime view over the read-only factory record
// (never copied), the user's hardware-zero profile and a calibration's trial: for each channel a
// trial at exactly its range and node wins, then the profile's nodes (when enabled), then the
// factory's, interpolated alike (interpolateCompare). generation() changes with every change of
// that answer, so a front end with the same settings still applies a new compare.
// It also holds the user's gain (volts per code only) and the ZT706S service interleave offset;
// neither touches the hardware, so neither changes generation(): captures take them when they are
// described (ChannelScale), so a record keeps the scale it was taken with.
class ScopeCalibration {
public:
    explicit ScopeCalibration(const CalibrationData &factory) : factory_(factory) {}
    const CalibrationData &factory() const { return factory_; }
    bool valid() const { return factory_.valid(); }
    uint32_t generation() const { return generation_; }
    bool offsetCompare(unsigned channel, unsigned range, int position, uint16_t &compare) const;
    // The node in force without a trial: the profile's when enabled, else the factory's.
    bool node(unsigned channel, unsigned range, unsigned point, uint16_t &compare) const;
    // A hardware zero is in force: the software zero (scope_zero.json) is then not added, so the
    // two never correct the same offset twice.
    bool pwmActive() const { return pwm_.enabled; }
    // The software zero's correction a capture adds to its zero code: none under a hardware zero.
    double softwareZero(const ZeroProfile &software, unsigned channel, unsigned range, int position) const {
        return pwm_.enabled ? 0 : software.correction(channel, range, position);
    }
    const PwmProfile &pwm() const { return pwm_; }
    // The factory transfer of a channel at a (physical) range and position, its volts per code x
    // the user gain of that range, once; the zero code the factory transfer's (no gain moves it).
    // displayGain: an interleaved CH1's absolute frequency gain in place of the calibration gain
    // (0: the calibration gain), volts per code only.
    bool scale(unsigned channel, unsigned range, int position, double &voltsPerCode, double &zeroCode,
               double displayGain = 0) const;
    const GainProfile &gain() const { return gain_; }
    bool setGain(const GainProfile &profile); // An enabled profile must be valid; a disabled one is kept as values.
    // The HIGH ADC's offset of a new interleaved record: the service profile when enabled, else the
    // factory record's (0 when that is no allowed offset).
    float interleaveOffset() const;
    const InterleaveProfile &interleave() const { return interleave_; }
    bool setInterleave(const InterleaveProfile &profile);
    // An enabled profile must be valid for this factory record; a disabled one is kept as values.
    bool setPwm(const PwmProfile &profile);
    void setTrial(unsigned channel, unsigned range, unsigned point, uint16_t compare);
    void clearTrial();

private:
    struct Trial { bool active = false; unsigned range = 0, point = 0; uint16_t compare = 0; };
    const CalibrationData &factory_;
    PwmProfile pwm_;
    GainProfile gain_;
    InterleaveProfile interleave_;
    Trial trial_[PwmProfile::Channels];
    uint32_t generation_ = 1;
};
} // namespace openosc::calibration
