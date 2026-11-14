#pragma once
#include "openosc/measurement/dmm/relative.hpp"
#include "openosc/measurement/dmm/scale.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace openosc::dmm {
struct BarReading {
    static constexpr int FullScale = BoardScale.fullScale; // The board's display maximum (scale.hpp).
    int counts = 0;
    DataStatus status = DataStatus::NoData;
    static BarReading from(const Result &result, DataStatus health,
                           const RelativeReading *relative = nullptr) {
        BarReading bar;
        bar.status = health == DataStatus::Ok ? result.primary.status : health;
        if (bar.status == DataStatus::Overload) {
            bar.counts = result.mainText[0] == '-' ? -FullScale : FullScale;
            return bar;
        }
        if (bar.status != DataStatus::Ok) return bar;
        if (!result.primary.valid()) { bar.status = DataStatus::Unsupported; return bar; }
        double magnitude = result.rawMagnitude;
        bool negative = result.primary.si < 0;
        if (relative && relative->matches(result)) {
            NumericValue unit;
            unsigned decimals = result.decimals;
            if (decimals == 255) {
                const char *dot = std::strchr(result.mainText, '.');
                decimals = dot ? std::strlen(dot + 1) : 0;
            }
            if (decimals > 4 || !UnitParser::parse("1", result.unit, unit) || unit.si <= 0) {
                bar.status = DataStatus::Unsupported; return bar;
            }
            double value = relative->apply(result).si / unit.si;
            negative = value < 0;
            for (unsigned i = 0; i < decimals; ++i) value *= 10;
            magnitude = std::abs(value);
        }
        if (!std::isfinite(magnitude)) { bar.status = DataStatus::Unsupported; return bar; }
        bar.counts = int(std::lround(std::clamp(magnitude, 0., double(FullScale))));
        if (negative) bar.counts = -bar.counts;
        return bar;
    }
};
} // namespace openosc::dmm
