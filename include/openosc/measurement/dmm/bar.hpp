#pragma once
#include "openosc/measurement/dmm/relative.hpp"
#include "openosc/measurement/dmm/resolution.hpp"
#include "openosc/measurement/dmm/scale.hpp"
#include <algorithm>
#include <cmath>

namespace openosc::dmm {
// The magnitude the bar and the dial show, in counts of the reading's own scale (scaleFor: the
// board's, capacitance its 9999-count one), clamped to its full scale; overload pegs it with its sign.
struct BarReading {
    CountScale scale = BoardScale;
    int counts = 0;
    DataStatus status = DataStatus::NoData;
    // The share of full scale (0..1) the magnitude reaches.
    float fraction() const { return float(std::abs(counts)) / float(scale.fullScale); }
    static BarReading from(const Result &result, DataStatus health,
                           const RelativeReading *relative = nullptr) {
        BarReading bar;
        bar.scale = scaleFor(result.primary.quantity);
        const int full = bar.scale.fullScale;
        bar.status = health == DataStatus::Ok ? result.primary.status : health;
        if (bar.status == DataStatus::Overload) {
            bar.counts = result.mainText[0] == '-' ? -full : full;
            return bar;
        }
        if (bar.status != DataStatus::Ok) return bar;
        if (!result.primary.valid()) { bar.status = DataStatus::Unsupported; return bar; }
        double magnitude = result.rawMagnitude;
        bool negative = result.primary.si < 0;
        if (relative && relative->matches(result)) {
            NumericValue unit;
            const unsigned decimals = Resolution::decimals(result);
            if (decimals > 4 || !UnitParser::parse("1", result.unit, unit) || unit.si <= 0) {
                bar.status = DataStatus::Unsupported; return bar;
            }
            double value = relative->apply(result).si / unit.si;
            negative = value < 0;
            for (unsigned i = 0; i < decimals; ++i) value *= 10;
            magnitude = std::abs(value);
        }
        if (!std::isfinite(magnitude)) { bar.status = DataStatus::Unsupported; return bar; }
        bar.counts = int(std::lround(std::clamp(magnitude, 0., double(full))));
        if (negative) bar.counts = -bar.counts;
        return bar;
    }
};
} // namespace openosc::dmm
