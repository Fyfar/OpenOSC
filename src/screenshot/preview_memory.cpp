#include "openosc/screenshot/reader.hpp"

namespace openosc::screenshot {
namespace {
alignas(32) uint16_t pixels[BitmapLayout::Width*BitmapLayout::Height]
    __attribute__((section(".image_preview")));
}
uint16_t *previewMemory() { return pixels; }
} // namespace openosc::screenshot
