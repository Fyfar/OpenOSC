#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class Binary {
public:
    static bool overlaps(const void *first, size_t firstBytes, const void *second, size_t secondBytes) {
        if (!firstBytes || !secondBytes)
            return false;
        const auto a = reinterpret_cast<uintptr_t>(first), b = reinterpret_cast<uintptr_t>(second);
        return a <= b ? b - a < firstBytes : a - b < secondBytes;
    }
    static uint64_t readLe(const uint8_t *data, unsigned bytes) {
        uint64_t value = 0;
        for (unsigned i = 0; i < bytes; ++i)
            value |= uint64_t(data[i]) << (i * 8);
        return value;
    }
    static void writeLe(uint8_t *data, uint64_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i)
            data[i] = uint8_t(value >> (i * 8));
    }
};
} // namespace openosc
