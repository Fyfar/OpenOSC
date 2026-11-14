#pragma once
#include <cstdint>

namespace openosc::scope {
// Probe presets shared by the channel editor and SCPI. Stored ratios remain numeric.
class ProbeAttenuation {
public:
    static constexpr uint32_t Values[] = {1, 10, 50, 100, 500, 1000};
    static constexpr unsigned Count = sizeof Values / sizeof Values[0];
    static constexpr bool contains(double factor) {
        for (auto value : Values) if (value == factor) return true;
        return false;
    }
    // The adjacent preset to a saved ratio, wrapping at either end. Comparing fractions keeps
    // non-preset ratios from settings meaningful until the user adjusts them.
    static constexpr uint32_t step(uint32_t numerator, uint32_t denominator, int direction) {
        if (!direction) return numerator;
        if (direction > 0) {
            for (auto value : Values) if (uint64_t(value) * denominator > numerator) return value;
            return Values[0];
        }
        for (unsigned i = Count; i-- > 0;)
            if (uint64_t(Values[i]) * denominator < numerator) return Values[i];
        return Values[Count - 1];
    }
};
} // namespace openosc::scope
