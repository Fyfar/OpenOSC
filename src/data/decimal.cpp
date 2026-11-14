#include "openosc/data/decimal.hpp"
#include <cmath>
#include <cstring>

namespace openosc {
size_t Decimal::integer(uint64_t value, char *output, size_t capacity) {
    char reverse[20];
    size_t count = 0;
    do {
        reverse[count++] = char('0' + value % 10);
        value /= 10;
    } while (value);
    if (!output || capacity <= count)
        return 0;
    for (size_t i = 0; i < count; ++i)
        output[i] = reverse[count - 1 - i];
    output[count] = 0;
    return count;
}
size_t Decimal::fraction(uint64_t numerator, uint32_t denominator, char *output, size_t capacity,
                         unsigned digits) {
    if (!output || !denominator || digits > 18)
        return 0;
    char text[48];
    size_t count = integer(numerator / denominator, text, sizeof text);
    uint64_t remainder = numerator % denominator;
    if (remainder && digits) {
        text[count++] = '.';
        for (unsigned i = 0; i < digits; ++i) {
            remainder *= 10;
            text[count++] = char('0' + remainder / denominator);
            remainder %= denominator;
            if (!remainder)
                break;
        }
    }
    if (count >= capacity)
        return 0;
    text[count] = 0;
    std::memcpy(output, text, count + 1);
    return count;
}
size_t Decimal::number(double value, char *output, size_t capacity,unsigned significantDigits) {
    if (!output || !std::isfinite(value)||!significantDigits||significantDigits>12)
        return 0;
    if (value == 0)
        return integer(0, output, capacity);
    char text[56];
    size_t count = 0;
    if (value < 0) {
        text[count++] = '-';
        value = -value;
    }
    int exponent = 0;
    // Bounded scaling also handles subnormal doubles without pow underflow.
    while (value >= 10) {
        value /= 10;
        ++exponent;
    }
    while (value < 1) {
        value *= 10;
        --exponent;
    }
    uint64_t factor=1;for(unsigned i=1;i<significantDigits;++i)factor*=10;
    uint64_t mantissa = static_cast<uint64_t>(std::floor(value * factor + .5));
    if (mantissa >= factor*10) {
        mantissa /= 10;
        ++exponent;
    }
    char digits[24];
    const size_t length = integer(mantissa, digits, sizeof digits);
    text[count++] = digits[0];
    size_t end = length;
    while (end > 1 && digits[end - 1] == '0')
        --end;
    if (end > 1) {
        text[count++] = '.';
        for (size_t i = 1; i < end; ++i)
            text[count++] = digits[i];
    }
    text[count++] = 'e';
    text[count++] = exponent < 0 ? '-' : '+';
    count += integer(exponent < 0 ? -exponent : exponent, text + count, sizeof text - count);
    if (count >= capacity)
        return 0;
    text[count] = 0;
    std::memcpy(output, text, count + 1);
    return count;
}
} // namespace openosc
