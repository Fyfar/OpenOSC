#pragma once
#include <cstdint>

namespace openosc {
enum class DataStatus : uint8_t {
    Ok,
    NoData,
    Busy,
    InvalidArgument,
    Unsupported,
    InvalidPacket,
    Overload,
    Stale,
    IoError,
    Timeout,
    Overflow,
    HardwareError,
    Uncalibrated,
    NoSpace, // A file would not fit the free space; nothing was written.
    // A well-formed reading without a number: the DMM's own blanks, dashes or "--." (no measurement
    // yet, a range or function in transition). Not an error and never a value.
    NoReading
};
struct Rational {
    uint32_t numerator = 0, denominator = 1;
    bool valid() const { return numerator && denominator; }
    double value() const { return denominator ? double(numerator) / denominator : 0; }
    // Exactly this x multiply / divide, reduced; false when that does not fit 32 bits.
    bool scaled(uint32_t multiply, uint32_t divide, Rational &out) const {
        if (!valid() || !multiply || !divide) return false;
        uint64_t n = uint64_t(numerator) * multiply, d = uint64_t(denominator) * divide;
        uint64_t a = n, b = d;
        while (b) { const uint64_t t = a % b; a = b; b = t; }
        n /= a; d /= a;
        if (n > UINT32_MAX || d > UINT32_MAX) return false;
        out = {uint32_t(n), uint32_t(d)};
        return true;
    }
};
struct WideRational {
    uint64_t numerator = 0, denominator = 1;
    bool valid() const { return numerator && denominator; }
    double value() const { return denominator ? double(numerator) / denominator : 0; }
};
} // namespace openosc
