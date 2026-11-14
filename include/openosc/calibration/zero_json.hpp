#pragma once
#include "openosc/calibration/zero_profile.hpp"
#include <cstddef>

namespace openosc::calibration {
class ZeroJson {
public:
    static constexpr size_t MaxBytes=4096;
    static bool parse(const char *text,size_t bytes,uint16_t factoryCrc,ZeroProfile &output);
    static size_t encode(const ZeroProfile &values,char *output,size_t capacity);
};
} // namespace openosc::calibration
