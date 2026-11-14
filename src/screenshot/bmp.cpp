#include "openosc/screenshot/bmp.hpp"
#include <cstring>

namespace openosc {
namespace {
void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
void put32(uint8_t* p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
}
BmpImage::BmpImage(const uint16_t* pixels) : pixels_(pixels) {
    header_[0] = 'B'; header_[1] = 'M';
    put32(header_ + 2, FileBytes); put32(header_ + 10, HeaderBytes);
    put32(header_ + 14, 40); put32(header_ + 18, Width);
    put32(header_ + 22, uint32_t(-int32_t(Height))); // Top-down, same order as LTDC.
    put16(header_ + 26, 1); put16(header_ + 28, 16);
    put32(header_ + 30, 3); put32(header_ + 34, PixelBytes);
    put32(header_ + 54, 0xf800); put32(header_ + 58, 0x07e0); put32(header_ + 62, 0x001f);
}
size_t BmpImage::read(uint32_t offset, void* output, size_t count) const {
    if (!pixels_ || offset >= FileBytes) return 0;
    if (count > FileBytes - offset) count = FileBytes - offset;
    auto* target = static_cast<uint8_t*>(output);
    const size_t total = count;
    if (offset < HeaderBytes) {
        size_t bytes = HeaderBytes - offset;
        if (bytes > count) bytes = count;
        std::memcpy(target, header_ + offset, bytes);
        target += bytes; offset += bytes; count -= bytes;
    }
    if (count) std::memcpy(target, reinterpret_cast<const uint8_t*>(pixels_) + offset - HeaderBytes, count);
    return total;
}
}
