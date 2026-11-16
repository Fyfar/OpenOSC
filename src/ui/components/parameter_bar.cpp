#include "openosc/ui/components/parameter_bar.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace openosc::ui {
namespace {
// LVGL invalidates on every style write, equal or not: only a colour that differs is written, so a
// context or selection that did not change draws nothing.
void border(lv_obj_t *object, uint32_t color) {
    if (!lv_color_eq(lv_obj_get_style_border_color(object, LV_PART_MAIN), lv_color_hex(color)))
        lv_obj_set_style_border_color(object, lv_color_hex(color), 0);
}
void ink(lv_obj_t *object, uint32_t color) {
    if (!lv_color_eq(lv_obj_get_style_text_color(object, LV_PART_MAIN), lv_color_hex(color)))
        lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
}
} // namespace
lv_obj_t *ParameterBar::create(lv_obj_t *parent) {
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_scrollable(root_, false);
    selected_ = screenSelected_ = Count;
    context_ = false;
    for (unsigned i = 0; i < Count; ++i) { screen_[i] = {}; live_[i] = true; for (auto *&child : hidden_[i]) child = nullptr; }
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
    for (unsigned i = 0; i < Count; ++i) { // Equal zones: one per key under the screen.
        const int x = int(i) * available / int(Count) + int(i) * gap;
        const int w = (int(i) + 1) * available / int(Count) - int(i) * available / int(Count);
        widths_[i] = w;
        lv_obj_set_pos(cards_[i], x, 0);
        lv_obj_set_size(cards_[i], w, height);
        lv_obj_set_pos(titles_[i], 4, 1);
        lv_obj_set_size(titles_[i], std::max(1, w - 8), 14);
        const int line = lv_font_get_line_height(lv_obj_get_style_text_font(values_[i], LV_PART_MAIN));
        const int top = height >= line + 16 ? 16 : 1;
        lv_obj_set_hidden(titles_[i], top == 1);
        lv_obj_set_pos(values_[i], 4, top);
        lv_obj_set_size(values_[i], std::max(1, w - 8), std::max(1, std::min(line, height - top)));
        fit(i);
    }
}
void ParameterBar::fit(unsigned i) {
    if (customFont_) return; // The DMM's Card text font is the user's choice.
    for (auto *label : {titles_[i], values_[i]}) {
        const auto *current = lv_obj_get_style_text_font(label, LV_PART_MAIN);
        if (current != Elements::font(12) && current != Elements::font(11)) continue; // Another theme size: kept.
        const auto *font = Elements::textWidth(lv_label_get_text(label), 12) <= widths_[i] - 8 ? Elements::font(12) : Elements::font(11);
        if (current != font) lv_obj_set_style_text_font(label, font, 0);
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
    muted_ = theme.muted;
    selectedText_ = customFont_ = false;
    if (screenSelected_ >= Count) screenSelected_ = 0;
    if (!context_) selected_ = screenSelected_;
    for (unsigned i = 0; i < Count; ++i) {
        lv_obj_set_style_bg_color(cards_[i], lv_color_hex(background_), 0);
        lv_obj_set_style_text_font(values_[i], Elements::font(style.font), 0);
        lv_obj_set_style_text_font(titles_[i], Elements::font(12), 0);
    }
    paint();
    arrange();
}
void ParameterBar::text(const lv_font_t *font, uint32_t color, bool selected) {
    foreground_ = color;
    selectedText_ = selected;
    customFont_ = true;
    for (unsigned i = 0; i < Count; ++i) {
        if (lv_obj_get_style_text_font(values_[i], LV_PART_MAIN) != font) lv_obj_set_style_text_font(values_[i], font, 0);
        if (lv_obj_get_style_text_font(titles_[i], LV_PART_MAIN) != font) lv_obj_set_style_text_font(titles_[i], font, 0);
    }
    paint();
    arrange();
}
// The layer on show: the accented card's border and value in the highlight, a marked title in the
// highlight, an inert key's texts muted.
void ParameterBar::paint() {
    for (unsigned i = 0; i < Count; ++i) {
        const bool accent = i == selected_ && live_[i];
        border(cards_[i], accent ? highlight_ : border_);
        ink(values_[i], !live_[i] ? muted_ : accent && !selectedText_ ? highlight_ : foreground_);
        ink(titles_[i], !context_ && screen_[i].marked ? highlight_ : muted_);
    }
}
void ParameterBar::select(unsigned index) {
    screenSelected_ = index;
    if (context_ || selected_ == index) return;
    selected_ = index;
    paint();
}
void ParameterBar::show(unsigned index, const char *title, const char *value) {
    if (std::strcmp(lv_label_get_text(titles_[index]), title)) { lv_label_set_text(titles_[index], title); fit(index); }
    if (std::strcmp(lv_label_get_text(values_[index]), value)) { lv_label_set_text(values_[index], value); fit(index); }
}
void ParameterBar::item(unsigned index, const char *title, const char *value, bool marked) {
    if (index >= Count) return;
    auto &card = screen_[index];
    std::snprintf(card.title, sizeof card.title, "%s", title);
    std::snprintf(card.value, sizeof card.value, "%s", value);
    const bool remark = card.marked != marked;
    card.marked = marked;
    if (context_) return; // Kept for release().
    show(index, card.title, card.value);
    if (remark) paint();
}
void ParameterBar::decorations(bool visible) {
    for (unsigned i = 0; i < Count; ++i) {
        if (!visible) {
            unsigned n = 0;
            for (uint32_t c = 0; c < lv_obj_get_child_count(cards_[i]) && n < Decorations; ++c) {
                auto *child = lv_obj_get_child(cards_[i], int32_t(c));
                if (child == titles_[i] || child == values_[i] || lv_obj_is_hidden(child)) continue;
                lv_obj_set_hidden(child, true);
                hidden_[i][n++] = child;
            }
        } else {
            for (auto *&child : hidden_[i]) { if (child) lv_obj_set_hidden(child, false); child = nullptr; }
        }
    }
}
void ParameterBar::decorate(lv_obj_t *child, bool visible) {
    if (!child) return;
    for (unsigned i = 0; i < Count; ++i) {
        if (lv_obj_get_parent(child) != cards_[i]) continue;
        unsigned free = Decorations;
        for (unsigned n = 0; n < Decorations; ++n) {
            if (hidden_[i][n] == child) { if (!visible) hidden_[i][n] = nullptr; lv_obj_set_hidden(child, true); return; }
            if (!hidden_[i][n] && free == Decorations) free = n;
        }
        if (context_ && visible && free < Decorations) { hidden_[i][free] = child; lv_obj_set_hidden(child, true); return; } // At release().
    }
    if (lv_obj_is_hidden(child) == visible) lv_obj_set_hidden(child, !visible);
}
void ParameterBar::context(const SoftKeys &keys) {
    if (!context_) decorations(false);
    context_ = true;
    for (unsigned i = 0; i < Count; ++i) {
        live_[i] = keys.keys[i].live;
        show(i, keys.keys[i].title, keys.keys[i].value);
    }
    selected_ = keys.selected < Count && keys.keys[keys.selected].live ? keys.selected : Count;
    paint();
}
void ParameterBar::release() {
    if (!context_) return;
    context_ = false;
    for (unsigned i = 0; i < Count; ++i) {
        live_[i] = true;
        show(i, screen_[i].title, screen_[i].value);
    }
    decorations(true);
    selected_ = screenSelected_;
    paint();
}
} // namespace openosc::ui
