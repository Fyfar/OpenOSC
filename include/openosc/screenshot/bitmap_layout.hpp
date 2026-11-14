#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc::screenshot {
// The viewer accepts the instrument's RGB565 screenshots and ordinary 24-bit BMPs.
// Parsing is independent of FatFs and never trusts the declared file size.
struct BitmapLayout {
    static constexpr unsigned Width=320, Height=240, PreviewWidth=256, PreviewHeight=192;
    uint32_t offset=0, stride=0;
    uint16_t bits=0;
    bool topDown=false;
    static bool parse(const uint8_t *header,size_t bytes,uint64_t fileBytes,BitmapLayout &output);
    uint32_t rowOffset(unsigned row) const { return offset+(topDown ? row : Height-1-row)*stride; }
    void previewRow(const uint8_t *source,uint16_t *target, bool fullSize = false) const;
};
} // namespace openosc::screenshot
