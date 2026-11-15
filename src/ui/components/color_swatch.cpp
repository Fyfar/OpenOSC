#include "openosc/ui/components/color_swatch.hpp"

namespace openosc::ui {
void ColorSwatch::update(lv_obj_t *parent, int x, int y, uint32_t rgb, uint32_t frame) {
    if (!box_) {
        box_ = lv_obj_create(parent);
        lv_obj_remove_style_all(box_);
        lv_obj_set_ignore_layout(box_, true); // Beside the row's grid cells, not in them.
        lv_obj_set_clickable(box_, false);
        lv_obj_set_scrollable(box_, false);
        lv_obj_set_size(box_, Size, Size);
        lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(box_, 1, 0);
    }
    lv_obj_set_pos(box_, x, y);
    lv_obj_set_style_bg_color(box_, lv_color_hex(rgb), 0);
    lv_obj_set_style_border_color(box_, lv_color_hex(frame), 0);
    lv_obj_set_hidden(box_, false);
}
} // namespace openosc::ui
