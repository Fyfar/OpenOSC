#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class Decimal {
public:
    static size_t integer(uint64_t value, char *output, size_t capacity);
    static size_t fraction(uint64_t numerator, uint32_t denominator, char *output, size_t capacity,
                           unsigned digits = 12);
    static size_t number(double value, char *output, size_t capacity,unsigned significantDigits=12);
    static size_t fixed(double value, unsigned decimals, char *output, size_t capacity);
};
} // namespace openosc
