#pragma once
#include <cstdint>

namespace openosc::scope {
class VoltageRanges {
public:
    static constexpr uint32_t Values[] = {20, 40, 50, 100, 200, 500, 1000, 2000, 2500, 5000, 10000};
    static constexpr unsigned Count = sizeof(Values) / sizeof(*Values);
    static bool index(uint32_t millivolts, unsigned &output) {
        for (unsigned i = 0; i < Count; ++i)
            if (Values[i] == millivolts) {
                output = i;
                return true;
            }
        return false;
    }
    static bool contains(uint32_t millivolts) {
        unsigned result;
        return index(millivolts, result);
    }
    static uint32_t millivolts(unsigned index) { return index < Count ? Values[index] : 0; }
};
} // namespace openosc::scope
