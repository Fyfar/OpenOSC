#include "openosc/acquisition/ranges.hpp"
#include "openosc/calibration/data.hpp"

namespace openosc {
bool CalibrationData::scopeScale(unsigned channel, uint32_t millivolts, int position, double &volts,
                                 double &zero, double displayGain) const {
    if (!factoryScale(channel) || position < 0 || position > 160 || !scope::VoltageRanges::contains(millivolts) ||
        !(displayGain == 0 || (displayGain >= 1.0 && displayGain <= 1.99)))
        return false;
    // Factory transfer: 20 pixels/div and Y = 80 - .625 * gain * signed_code.
    // The offset table calibrates the zero-input code to the requested Y position.
    const double gain = this->displayGain(channel); // The word / 100 in double; an unwritten word: 1.20.
    const double pixelsPerCode = .625 * gain;
    volts = millivolts / 1000.0 * .625 * (displayGain ? displayGain : gain) / 20;
    zero = (80 - position) / pixelsPerCode;
    return true;
}
} // namespace openosc
