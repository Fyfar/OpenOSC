#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc::generator {
// Offline 706S wire format. It does not configure the 703S power-button pin as USART3.
class SerialFrameCodec {
public:
    static constexpr size_t Bytes = 512, MaximumSamples = 255;
    static bool encode(const uint8_t *samples, size_t count, uint32_t divisor, uint8_t *output,
                       size_t capacity);
    static bool decode(const uint8_t *input, size_t bytes, uint8_t *samples, size_t capacity, size_t &count,
                       uint32_t &divisor);
};
} // namespace openosc::generator
