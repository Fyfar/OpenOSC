#include "openosc/data/memory.hpp"
#include "lvgl.h"

namespace openosc {
bool GuiHeap::read(MemorySnapshot &output) {
    if (!lv_is_initialized()) return false;
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    if (!monitor.total_size || monitor.free_size > monitor.total_size) return false;
    output.total = uint32_t(monitor.total_size);
    output.free = uint32_t(monitor.free_size);
    output.used = uint32_t(monitor.total_size - monitor.free_size);
    output.peak = uint32_t(monitor.max_used);
    output.largestFree = uint32_t(monitor.free_biggest_size);
    output.fragmentationPct = monitor.frag_pct;
    return true;
}
} // namespace openosc
