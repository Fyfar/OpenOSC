#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class BmpImage {
public:
    static constexpr uint32_t Width = 320, Height = 240, HeaderBytes = 66;
    static constexpr uint32_t PixelBytes = Width * Height * 2, FileBytes = HeaderBytes + PixelBytes;
    explicit BmpImage(const uint16_t* pixels);
    size_t read(uint32_t offset, void* output, size_t count) const;
private:
    const uint16_t* pixels_;
    uint8_t header_[HeaderBytes]{};
};
}
