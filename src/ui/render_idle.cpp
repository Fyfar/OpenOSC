#include "openosc/ui/render_idle.hpp"
#include "lvgl.h"

namespace openosc {
namespace {
RenderIdle::Callback callback = nullptr;
void *owner = nullptr;
uint32_t previous = 0;
bool running = false;
} // namespace
void RenderIdle::install(Callback function, void *context) {
    callback = function; owner = context; previous = 0;
}
void RenderIdle::remove(void *context) {
    if (owner == context) { callback = nullptr; owner = nullptr; }
}
void RenderIdle::run(uint32_t now) {
    if (!callback || running || now == previous) return;
    previous = now;
    running = true;
    callback(owner, now);
    running = false;
}
} // namespace openosc

extern "C" void openosc_render_idle(void) { openosc::RenderIdle::run(lv_tick_get()); }
