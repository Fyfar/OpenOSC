#include "openosc/calibration/pwm_profile.hpp"
#include "openosc/acquisition/ranges.hpp"
#include <cmath>

namespace openosc::calibration {
namespace {
// +1: strictly increasing, -1: strictly decreasing, 0: neither.
int direction(const uint16_t (&row)[PwmProfile::Points]) {
    const int sign = row[1] > row[0] ? 1 : row[1] < row[0] ? -1 : 0;
    for (unsigned p = 1; p < PwmProfile::Points; ++p)
        if (sign == 0 || (row[p] > row[p - 1] ? 1 : row[p] < row[p - 1] ? -1 : 0) != sign) return 0;
    return sign;
}
} // namespace
bool PwmGeometry::target(const CalibrationData &factory, unsigned channel, unsigned range, unsigned point, double &code) {
    double volts = 0;
    return point < PwmProfile::Points && range < PwmProfile::Ranges &&
           factory.scopeScale(channel, scope::VoltageRanges::millivolts(range), ZeroProfile::Positions[point], volts, code);
}
bool PwmGeometry::slope(const CalibrationData &factory, unsigned channel, unsigned range, unsigned point, double &codesPerCount) {
    uint16_t row[PwmProfile::Points];
    if (point >= PwmProfile::Points || !factory.offsetRow(channel, range, row) || !direction(row)) return false;
    const unsigned segment = point < PwmProfile::Points - 1 ? point : point - 1;
    double first = 0, second = 0;
    if (!target(factory, channel, range, segment, first) || !target(factory, channel, range, segment + 1, second)) return false;
    codesPerCount = (second - first) / (double(row[segment + 1]) - row[segment]);
    return std::isfinite(codesPerCount) && codesPerCount != 0;
}
bool PwmProfile::empty() const {
    for (const auto &channel : compare) for (const auto &range : channel) for (uint16_t value : range) if (value) return false;
    return true;
}
bool PwmProfile::valid(const CalibrationData &factory) const {
    if (!factory.valid() || factoryCrc != factory.crc()) return false;
    if (!enabled && empty()) return true;
    for (unsigned c = 0; c < Channels; ++c)
        for (unsigned r = 0; r < Ranges; ++r) {
            uint16_t row[Points];
            if (!factory.offsetRow(c, r, row)) return false;
            const int sign = direction(row);
            if (!sign || direction(compare[c][r]) != sign) return false;
            for (unsigned p = 0; p < Points; ++p) {
                double slope = 0;
                if (compare[c][r][p] > 9999 || !PwmGeometry::slope(factory, c, r, p, slope) ||
                    std::abs((double(compare[c][r][p]) - row[p]) * slope) > MaximumShiftCodes) return false;
            }
        }
    return true;
}
} // namespace openosc::calibration
