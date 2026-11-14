#include "openosc/calibration/zero_profile.hpp"
#include <cmath>

namespace openosc::calibration {
bool ZeroProfile::valid() const {
    for (const auto &channel:codes) for (const auto &range:channel) {
        for (float value:range) if (!std::isfinite(value) || std::abs(value)>MaximumCorrection) return false;
        if (std::abs(2*range[0]-range[1])>MaximumCorrection ||
            std::abs(2*range[Points-1]-range[Points-2])>MaximumCorrection) return false;
    }
    return true;
}
double ZeroProfile::correction(unsigned channel,unsigned range,int position) const {
    if (!enabled || channel>=Channels || range>=Ranges || position<0 || position>160) return 0;
    const unsigned segment=position<=20 ? 0 : position>=140 ? Points-2 : unsigned(position-20)/20;
    const auto &points=codes[channel][range];
    return points[segment]+(double(points[segment+1])-points[segment])*(position-Positions[segment])/20;
}
} // namespace openosc::calibration
