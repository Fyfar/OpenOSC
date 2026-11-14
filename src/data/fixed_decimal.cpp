#include "openosc/data/decimal.hpp"
#include <cmath>
#include <cstring>

namespace openosc {
size_t Decimal::fixed(double value, unsigned decimals, char *output, size_t capacity) {
    if (!output || !std::isfinite(value) || decimals > 9) return 0;
    uint32_t scale = 1;
    for (unsigned i = 0; i < decimals; ++i) scale *= 10;
    const double rounded = std::floor(std::abs(value) * scale + .5);
    if (!std::isfinite(rounded) || rounded > 9007199254740991.0) return 0;
    const auto magnitude = uint64_t(rounded);
    char text[48]; size_t used = 0;
    if (value < 0 && magnitude) text[used++] = '-';
    used += integer(magnitude / scale, text + used, sizeof text - used);
    if (decimals) {
        text[used++] = '.';
        uint32_t remainder = magnitude % scale;
        for (unsigned divisor = scale / 10; divisor; divisor /= 10) {
            text[used++] = '0' + remainder / divisor; remainder %= divisor;
        }
    }
    if (used >= capacity) return 0;
    text[used] = 0; std::memcpy(output, text, used + 1); return used;
}
} // namespace openosc
