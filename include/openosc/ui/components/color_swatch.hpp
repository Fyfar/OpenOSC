#pragma once
#include "lvgl.h"
#include <cstdint>

namespace openosc::ui {
// A small colour sample beside an option or a value: a 12x12 fill with a 1 px frame in the row's
// own text colour, so black and white samples show on any panel and on the selected row. Created
// once with its row and only hidden when the row is no colour, so redraws add no objects.
class ColorSwatch {
public:
    static constexpr int Size = 12, Gap = 4;
    void update(lv_obj_t *parent, int x, int y, uint32_t rgb, uint32_t frame);
    void hide() { if (box_) lv_obj_set_hidden(box_, true); }
    void reset() { box_ = nullptr; } // Its parent was cleaned.
private:
    lv_obj_t *box_ = nullptr;
};
} // namespace openosc::ui
