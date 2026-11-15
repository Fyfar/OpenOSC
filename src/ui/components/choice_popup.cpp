#include "openosc/ui/components/choice_popup.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::ui {
void ChoicePopup::show(lv_obj_t *menu, int top, const PropertyList &list, unsigned row, unsigned draft, const Theme &theme, uint32_t accent) {
    constexpr int RowHeight = 24, Width = 200;
    if (!box_) {
        box_ = lv_obj_create(menu);
        lv_obj_remove_style_all(box_);
        lv_obj_set_scrollable(box_, false);
        counter_ = Elements::label(box_, "", 0, 2);
    }
    const unsigned options = list.options(row), current = list.option(row);
    const unsigned first = draft / Visible * Visible, shown = options - first < Visible ? options - first : Visible;
    lv_obj_set_pos(box_, 304 - Width - 8, top);
    lv_obj_set_size(box_, Width, 22 + int(shown) * RowHeight);
    lv_obj_set_style_bg_color(box_, lv_color_hex(theme.panel), 0);
    lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box_, 1, 0);
    lv_obj_set_style_border_color(box_, lv_color_hex(accent), 0);
    lv_obj_set_style_radius(box_, 3, 0);
    lv_obj_set_hidden(box_, false);
    lv_obj_move_foreground(box_);
    char page[16];
    std::snprintf(page, sizeof page, "%u/%u", draft + 1, options);
    if (std::strcmp(lv_label_get_text(counter_), page)) lv_label_set_text(counter_, page);
    lv_obj_set_pos(counter_, 8, 3);
    lv_obj_set_style_text_color(counter_, lv_color_hex(theme.muted), 0);
    for (unsigned i = 0; i < Visible; ++i) {
        if (i >= shown) { rows_[i].visible(false); continue; }
        char name[48], shown[52];
        list.optionName(row, first + i, name, sizeof name);
        std::snprintf(shown, sizeof shown, first + i == current ? "%s <" : "%s", name); // The option in force.
        rows_[i].update(box_, 20 + int(i) * RowHeight, RowHeight - 2, shown, "", first + i == draft, theme, accent, 2, Width - 6);
        if (uint32_t rgb; list.optionSwatch(row, first + i, rgb)) rows_[i].nameSwatch(rgb); // Colour lists only.
    }
}
} // namespace openosc::ui
