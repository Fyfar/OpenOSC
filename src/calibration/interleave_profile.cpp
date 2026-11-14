#include "openosc/calibration/interleave_profile.hpp"
#include <cmath>

namespace openosc::calibration {
bool InterleaveProfile::valid(const CalibrationData &factory) const {
    return factory.valid() && factoryCrc == factory.crc() && std::isfinite(offset) && allowed(offset);
}
} // namespace openosc::calibration
