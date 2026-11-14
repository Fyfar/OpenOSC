#pragma once
#include "openosc/data/status.hpp"
#include <cstdint>
#include <numeric>

namespace openosc::generator {
// Generator frequencies cross the UI, JSON and SCPI in millihertz as uint64 (the ZT706S module goes to
// 10 MHz = 1e10 mHz, beyond uint32). Settings keep a Rational (uint32/uint32) for the timer plans.
struct Frequency {
    // Reduced millihertz/1000. A value that does not fit even reduced (above 4.29 MHz with a fraction
    // of a hertz) becomes the nearest whole hertz, which every supported frequency fits.
    static Rational rational(uint64_t millihertz) {
        const uint64_t common = std::gcd(millihertz, uint64_t(1000));
        const uint64_t numerator = millihertz / common;
        if (numerator <= 0xffffffffULL) return {uint32_t(numerator), uint32_t(1000 / common)};
        return {uint32_t((millihertz + 500) / 1000), 1};
    }
    static uint64_t millihertz(const Rational &hertz) {
        if (!hertz.denominator) return 0;
        return (uint64_t(hertz.numerator) * 1000 + hertz.denominator / 2) / hertz.denominator;
    }
};
} // namespace openosc::generator
