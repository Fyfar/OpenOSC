#include "openosc/ui/components/decode_strip.hpp"
#include "openosc/ui/fonts.hpp"
#include "openosc/decode/table_text.hpp"
#include <algorithm>
#include <cstring>

namespace openosc::ui {
namespace {
void box(lv_layer_t *layer, const lv_area_t &area, uint32_t fill, uint32_t border, int width) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(fill); d.bg_opa = LV_OPA_COVER;
    d.border_color = lv_color_hex(border); d.border_width = width; d.border_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &d, &area);
}
} // namespace
bool DecodeStrip::columns(const plot::Viewport &viewport, uint32_t first, uint32_t last, int &x1, int &x2) {
    if (!viewport.spanQ16 || viewport.width < 2 || last <= first) return false;
    const int a = viewport.sampleX(first), b = viewport.sampleX(last - 1); // Its own last point (`last` is the next one's first).
    if (b < 0 || a >= viewport.width) return false;
    x1 = std::max(a, 0); x2 = std::min(std::max(b, a), int(viewport.width) - 1);
    return true;
}
void DecodeStrip::create(lv_obj_t *parent) {
    object_ = lv_obj_create(parent);
    lv_obj_remove_style_all(object_);
    lv_obj_set_scrollable(object_, false);
    lv_obj_add_event_cb(object_, draw, LV_EVENT_DRAW_MAIN, this);
    set_ = nullptr; revision_ = 0xFFFFFFFFu;
    lv_obj_set_hidden(object_, true);
}
void DecodeStrip::colors(const Theme &theme) {
    foreground_ = theme.foreground; background_ = theme.background; accent_ = theme.accent; warning_ = theme.generator;
    if (object_) lv_obj_invalidate(object_);
}
void DecodeStrip::place(int x, int y, int width, int height) {
    lv_obj_set_pos(object_, x, y);
    lv_obj_set_size(object_, width, height);
    if (lv_obj_is_hidden(object_)) lv_obj_set_hidden(object_, false);
    lv_obj_move_foreground(object_); // Over the elements (Elements::apply raises them on each activation).
}
void DecodeStrip::hide() { if (object_ && !lv_obj_is_hidden(object_)) lv_obj_set_hidden(object_, true); }
void DecodeStrip::show(const decoding::ResultSet *set, const decoding::ResultSets *sets, uint32_t revision, const plot::Viewport &viewport) {
    if (!object_) return;
    if (set == set_ && sets == sets_ && revision == revision_ && viewport == viewport_) return;
    set_ = set; sets_ = sets; revision_ = revision; viewport_ = viewport;
    lv_obj_invalidate(object_);
}
void DecodeStrip::draw(lv_event_t *event) {
    const auto &strip = *static_cast<DecodeStrip *>(lv_event_get_user_data(event));
    auto *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(strip.object_, &area);
    box(layer, area, strip.background_, strip.background_, 0);
    if (!strip.set_ || !strip.sets_ || strip.sets_->published() != strip.set_ || strip.sets_->revision() != strip.revision_)
        return; // Published again or cleared since show(): never another record's events.
    int textEnd = -1; // Texts never overlap: each starts where the one before ends, or past it.
    for (unsigned i = 0; i < strip.set_->count(); ++i) {
        const auto &e = strip.set_->event(i);
        int a, b;
        if (!columns(strip.viewport_, e.first, e.last, a, b)) continue;
        const int x1 = area.x1 + a, x2 = area.x1 + b;
        const bool error = decoding::TableText::error(e); // Said by its code too, never by colour alone.
        const uint32_t frame = error ? strip.warning_ : strip.accent_;
        if (b - a + 1 >= TextWidth && a >= textEnd) { // Neighbours may share their boundary column.
            box(layer, {x1, area.y1, x2, area.y2}, strip.background_, frame, 1);
            char label[12];
            decoding::TableText::label(*strip.set_, i, label, sizeof label);
            const size_t fits = size_t(b - a - 1) / CharWidth; // Inside its own frame only.
            if (std::strlen(label) > fits) label[fits] = 0;
            lv_draw_label_dsc_t text;
            lv_draw_label_dsc_init(&text);
            text.font = &openosc_mono_12;
            text.color = lv_color_hex(error ? strip.warning_ : strip.foreground_);
            text.text = label;
            text.text_local = 1;
            text.flag = LV_TEXT_FLAG_EXPAND;
            text.align = LV_TEXT_ALIGN_CENTER;
            const lv_area_t place = {x1, area.y1 - 1, x2, area.y2};
            lv_draw_label(layer, &text, &place);
            textEnd = b;
        } else if (strip.viewport_.sampleX(e.first) >= 0) {
            box(layer, {x1, area.y1, x1, area.y2}, frame, frame, 0); // A tick at its start.
        }
    }
}
} // namespace openosc::ui
