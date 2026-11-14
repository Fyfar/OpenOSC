#include "openosc/calibration/gain_profile.hpp"
#include <cmath>

namespace openosc::calibration {
bool GainProfile::valid(const CalibrationData &factory) const {
    if (!factory.valid() || factoryCrc != factory.crc()) return false;
    for (const auto &channel : gain) for (float factor : channel) if (!std::isfinite(factor) || !allowed(factor)) return false;
    return true;
}
double GainProfile::factor(unsigned channel, unsigned range) const {
    return enabled && channel < Channels && range < Ranges ? gain[channel][range] : 1.0;
}
} // namespace openosc::calibration
