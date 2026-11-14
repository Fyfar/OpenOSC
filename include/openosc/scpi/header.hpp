#pragma once
#include <cstdint>

namespace openosc::scpi {
class Header {
public:
    static bool matches(const char *actual, const char *pattern, unsigned *suffix = nullptr);
    static bool word(const char *actual, const char *pattern);
    static bool boolean(const char *argument, bool &value);
    enum class Unit { Scalar, Voltage, Time, Frequency, Percent };
    static bool number(const char *argument, double &value, Unit unit = Unit::Scalar);
};
} // namespace openosc::scpi
