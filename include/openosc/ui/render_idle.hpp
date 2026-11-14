#pragma once
#include <cstdint>

namespace openosc {
// Work for the time LVGL waits on an asynchronous unit (a DMA2D transfer) inside a refresh:
// LVGL calls openosc_render_idle() whenever nothing can be dispatched. The callback must not
// touch LVGL or anything a draw task reads (the draw tasks already hold their coordinates);
// it is not reentered and runs at most once per millisecond.
class RenderIdle {
public:
    using Callback = void (*)(void *context, uint32_t now);
    static void install(Callback callback, void *context);
    static void remove(void *context);
    static void run(uint32_t now);
};
} // namespace openosc
