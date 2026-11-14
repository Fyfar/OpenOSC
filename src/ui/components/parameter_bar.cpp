#include "openosc/ui/components/parameter_bar.hpp"
#include <algorithm>
#include <cstring>

namespace openosc::ui {
lv_obj_t *ParameterBar::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_scrollable(root_, false);
    selected_ = Count;
    for (unsigned i = 0; i < Count; ++i) {
        cards_[i] = lv_obj_create(root_);
        lv_obj_remove_style_all(cards_[i]);
        lv_obj_set_scrollable(cards_[i], false);
        lv_obj_set_style_bg_opa(cards_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cards_[i], 1, 0);
        titles_[i] = Elements::label(cards_[i], "");
        values_[i] = Elements::label(cards_[i], "");
        lv_obj_set_style_text_font(titles_[i], Elements::font(12), 0);
        lv_obj_set_style_text_font(values_[i], Elements::font(12), 0);
    }
    lv_obj_add_event_cb(root_, resized, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_set_size(root_, 304, 32);
    return root_;
}
void ParameterBar::arrange() {
    const int width = lv_obj_get_width(root_), height = lv_obj_get_height(root_);
    const int gap = width >= 80 ? 4 : 0;
    const int available = std::max<int>(4, width - 3 * gap);
    const int first = firstWide_ && available >= 200 ? 116 : available / Count; // The rest share what is left.
    const int rest = available - first;
    for (unsigned i = 0; i < Count; ++i) {
        const int x = !i ? 0 : first + (i - 1) * rest / (Count - 1) + i * gap;
        const int w = !i ? first : i * rest / (Count - 1) - (i - 1) * rest / (Count - 1);
        lv_obj_set_pos(cards_[i], x, 0);
        lv_obj_set_size(cards_[i], w, height);
        lv_obj_set_pos(titles_[i], 4, 1);
        lv_obj_set_size(titles_[i], std::max(1, w - 8), 14);
        const int line = lv_font_get_line_height(lv_obj_get_style_text_font(values_[i], LV_PART_MAIN));
        const int top = height >= line + 16 ? 16 : 1;
        lv_obj_set_hidden(titles_[i], top == 1);
        lv_obj_set_pos(values_[i], 4, top);
        lv_obj_set_size(values_[i], std::max(1, w - 8), std::max(1, std::min(line, height - top)));
    }
}
void ParameterBar::resized(lv_event_t *event) {
    static_cast<ParameterBar *>(lv_event_get_user_data(event))->arrange();
}
void ParameterBar::palette(const Theme &theme, const ElementStyle &style) {
    background_ = theme.panel;
    border_ = theme.grid;
    foreground_ = theme.foreground;
    highlight_ = style.color;
    selectedText_ = false;
    const unsigned selected = selected_;
    selected_ = Count;
    for (unsigned i = 0; i < Count; ++i) {
        lv_obj_set_style_bg_color(cards_[i], lv_color_hex(background_), 0);
        lv_obj_set_style_text_color(titles_[i], lv_color_hex(theme.muted), 0);
        lv_obj_set_style_text_font(values_[i], Elements::font(style.font), 0);
    }
    select(selected < Count ? selected : 0);
    arrange();
}
void ParameterBar::text(const lv_font_t *font, uint32_t color, bool selected) {
    foreground_ = color;
    selectedText_ = selected;
    for (unsigned i = 0; i < Count; ++i) {
        if (lv_obj_get_style_text_font(values_[i], LV_PART_MAIN) != font) lv_obj_set_style_text_font(values_[i], font, 0);
        if (lv_obj_get_style_text_font(titles_[i], LV_PART_MAIN) != font) lv_obj_set_style_text_font(titles_[i], font, 0);
    }
    const unsigned current = selected_;
    selected_ = Count;
    select(current);
    arrange();
}
void ParameterBar::select(unsigned index) {
    if (selected_ == index) return;
    selected_ = index;
    for (unsigned i = 0; i < Count; ++i) {
        lv_obj_set_style_border_color(cards_[i], lv_color_hex(i == index ? highlight_ : border_), 0);
        lv_obj_set_style_text_color(values_[i], lv_color_hex(i == index && !selectedText_ ? highlight_ : foreground_), 0);
    }
}
void ParameterBar::item(unsigned index, const char *title, const char *value) {
    if (index >= Count) return;
    if (std::strcmp(lv_label_get_text(titles_[index]), title)) lv_label_set_text(titles_[index], title);
    if (std::strcmp(lv_label_get_text(values_[index]), value)) lv_label_set_text(values_[index], value);
}
} // namespace openosc::ui
