#pragma once
#include "openosc/calibration/scope_calibration.hpp"
#include "openosc/acquisition/frame.hpp"
#include "openosc/acquisition/request.hpp"

namespace openosc::calibration {
// How a capture describes one channel (DataAccess::scopeChannelFor), the one place the scale is
// composed, so that each part enters exactly once:
//   - volts per code: the transfer at the PHYSICAL range the front end is set to (the requested
//     one, or an interleaved CH1's range correction), with the calibration gain or, for an
//     interleaved CH1, the absolute frequency display gain in its place, x the user gain of that
//     physical range;
//   - zero code: the factory transfer's at the position (the ADC zero of the offset PWM; no gain
//     moves it) + the software zero of the physical range when allowed (not during a zero
//     calibration's trials, nothing under a hardware zero);
//   - millivoltsPerDivision: the requested display range;
//   - probe ratio: copied from the request; readers apply it once (volts at the tip). The stored
//     gain never contains it.
// The record keeps what it was described with: a later profile changes only new records.
class ChannelScale {
public:
    static scope::ChannelMetadata describe(const ScopeCalibration &calibration, const ZeroProfile &softwareZero,
                                           bool softwareAllowed, const ScopeRequest &request, unsigned channel,
                                           unsigned physicalRange, double displayGain = 0);
};
} // namespace openosc::calibration
