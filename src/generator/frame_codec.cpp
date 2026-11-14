#include "openosc/generator/frame_codec.hpp"
#include <cstring>

namespace openosc::generator {
namespace {
void put(uint8_t *p, uint32_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        p[i] = uint8_t(value >> (8 * i));
}
uint32_t get(const uint8_t *p, unsigned count) {
    uint32_t v = 0;
    for (unsigned i = 0; i < count; ++i)
        v |= uint32_t(p[i]) << (8 * i);
    return v;
}
uint16_t sum(const uint8_t *p, size_t count) {
    uint16_t value = 0;
    for (size_t i = 0; i < count; ++i)
        value += p[i];
    return value;
}
} // namespace
bool SerialFrameCodec::encode(const uint8_t *samples, size_t count, uint32_t divisor, uint8_t *out,
                              size_t capacity) {
    if (!samples || !out || !count || count > MaximumSamples || !divisor || capacity < Bytes)
        return false;
    std::memmove(out + 14, samples, count);
    std::memset(out, 0, 14);
    std::memset(out + 14 + count, 0, Bytes - 14 - count);
    out[0] = 0xf5;
    out[1] = 0x38;
    out[2] = 0x55;
    out[3] = 0x94;
    put(out + 4, count + 8, 2);
    put(out + 6, divisor, 4);
    put(out + 10, count, 4);
    put(out + 14 + count, sum(out + 6, count + 8), 2);
    return true;
}
bool SerialFrameCodec::decode(const uint8_t *in, size_t bytes, uint8_t *samples, size_t capacity,
                              size_t &count, uint32_t &divisor) {
    if (!in || !samples || bytes != Bytes || in[0] != 0xf5 || in[1] != 0x38 || in[2] != 0x55 || in[3] != 0x94)
        return false;
    const size_t n = get(in + 10, 4);
    const uint32_t d = get(in + 6, 4);
    if (!n || n > MaximumSamples || n > capacity || !d || get(in + 4, 2) != n + 8 ||
        get(in + 14 + n, 2) != sum(in + 6, n + 8))
        return false;
    for (size_t i = 16 + n; i < Bytes; ++i)
        if (in[i])
            return false;
    std::memmove(samples, in + 14, n);
    count = n;
    divisor = d;
    return true;
}
} // namespace openosc::generator
