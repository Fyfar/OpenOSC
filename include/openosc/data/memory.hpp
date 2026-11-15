#pragma once
#include <cstdint>

namespace openosc {
// The LVGL heap as its allocator reports it (lv_mem_monitor: one walk of the TLSF pool, run only on
// request, never per frame). Bytes; `peak` is the allocator's own high-water mark since lv_init.
struct MemorySnapshot {
    uint32_t total = 0, used = 0, peak = 0, free = 0, largestFree = 0;
    uint8_t fragmentationPct = 0; // 100 - largest free block / all free, in percent.
};
class GuiHeap {
public:
    // False, and `output` untouched, while LVGL is not initialized (headless, failed display).
    static bool read(MemorySnapshot &output);
};
} // namespace openosc
