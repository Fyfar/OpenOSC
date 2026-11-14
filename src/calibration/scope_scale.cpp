#include "openosc/acquisition/ranges.hpp"
#include "openosc/calibration/data.hpp"

namespace openosc {
bool CalibrationData::scopeScale(unsigned channel, uint32_t millivolts, int position, double &volts,
                                 double &zero, double displayGain) const {
    if (!valid_ || channel > 1 || position < 0 || position > 160 ||
        !scope::VoltageRanges::contains(millivolts) || gains_[channel] < 100 || gains_[channel] > 199 ||
        !(displayGain == 0 || (displayGain >= 1.0 && displayGain <= 1.99)))
        return false;
    // Factory transfer: 20 pixels/div and Y = 80 - .625 * gain * signed_code.
    // The offset table calibrates the zero-input code to the requested Y position.
    const double pixelsPerCode = .625 * (gains_[channel] / 100.0);
    volts = millivolts / 1000.0 * .625 * (displayGain ? displayGain : gains_[channel] / 100.0) / 20;
    zero = (80 - position) / pixelsPerCode;
    return true;
}
} // namespace openosc
