#include "openosc/update/crc32.hpp"

namespace openosc {
void Crc32::update(const void* data, size_t size) {
    auto* bytes = static_cast<const uint8_t*>(data);
    while (size--) {
        state_ ^= *bytes++;
        for (unsigned bit = 0; bit < 8; ++bit)
            state_ = (state_ >> 1) ^ ((state_ & 1) ? 0xedb88320U : 0U);
    }
}
uint32_t Crc32::compute(const void* data, size_t size) {
    Crc32 crc;
    crc.update(data, size);
    return crc.value();
}
}

