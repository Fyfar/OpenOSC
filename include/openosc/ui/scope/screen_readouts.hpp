#pragma once
#include "openosc/acquisition/readouts.hpp"
#include "openosc/ui/scope/model.hpp"

namespace openosc::plot {
// The Screen basis from a projection's ScreenSeries (about 320 values per channel, no record read):
// Vpp/Min/Max from the window's exact extremes; Mean/RMS/AC RMS/duty/frequency/period/pulses from
// the series, its own extremes giving the edge thresholds. Frequency, duty and pulses are unknown
// (never a value) when the columns do not resolve the signal (resolution()) or the frequency is
// above a quarter of the series rate.
class ScreenReadouts {
public:
    static scope::ReadoutResult calculate(const ScreenSeries &series, const scope::FrameMetadata &metadata,
                                          const scope::ReadoutSelection &selection, uint32_t now);
    // edges: frequency, duty and pulses are the signal's; count: no swing at the window's end is
    // hidden from the pulse count.
    struct Resolution { bool edges, count; };
    static Resolution resolution(const ScreenSeries &series, unsigned channel);
    // The uncertainty of frequency, period and duty from edges sampled at `samplesPerSecond`
    // (Measurements::bound): what a series of this rate resolves at worst, phase-locked or not.
    static void bound(scope::Measurements &values, double samplesPerSecond);
};
} // namespace openosc::plot
