#include "openosc/calibration/channel_scale.hpp"
#include "openosc/acquisition/ranges.hpp"

namespace openosc::calibration {
scope::ChannelMetadata ChannelScale::describe(const ScopeCalibration &calibration, const ZeroProfile &softwareZero,
                                              bool softwareAllowed, const ScopeRequest &request, unsigned c,
                                              unsigned physicalRange, double displayGain) {
    scope::ChannelMetadata channel;
    if (c > 1) return channel;
    channel.enabled = request.frontend.enabled[c];
    channel.coupling = request.frontend.coupling[c];
    channel.analogPosition = int16_t(request.frontend.position[c]);
    channel.millivoltsPerDivision = scope::VoltageRanges::millivolts(request.frontend.range[c]);
    channel.probeNumerator = request.probeNumerator[c];
    channel.probeDenominator = request.probeDenominator[c];
    channel.calibrationCrc = calibration.factory().crc();
    channel.calibrated = calibration.scale(c, physicalRange, request.frontend.position[c], channel.voltsPerCode, channel.zeroCode, displayGain);
    if (softwareAllowed)
        channel.zeroCode += calibration.softwareZero(softwareZero, c, physicalRange, request.frontend.position[c]);
    return channel;
}
} // namespace openosc::calibration
