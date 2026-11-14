#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class Crc16 {
public:
    static uint16_t compute(const uint8_t *data, size_t bytes) {
        uint16_t crc = 0xffff;
        for (size_t i = 0; i < bytes; ++i) {
            crc ^= data[i];
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);
        }
        return crc;
    }
};
} // namespace openosc
