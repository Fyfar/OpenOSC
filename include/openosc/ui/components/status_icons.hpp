#pragma once
#include "lvgl.h"

namespace openosc::ui {
// The status bar's USB and generator icons: 9x12 alpha maps (scripts/build_status_icons.py) the
// theme colour tints, in the room the words "USB" and "GEN" would take twice over.
class StatusIcons {
public:
    static constexpr int Width = 9, Height = 12;
    static lv_obj_t *usb(lv_obj_t *parent, int x, int y, uint32_t color);
    static lv_obj_t *generator(lv_obj_t *parent, int x, int y, uint32_t color);
};
} // namespace openosc::ui
