#include "openosc/ui/components/footer_panel.hpp"
#include "openosc/ui/components/elements.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace openosc::ui {
namespace {
using Layout = ScopeFooterLayout;
constexpr int Radius = 2;
} // namespace
bool FooterPanel::Text::operator==(const Text &b) const {
    return x == b.x && width == b.width && color == b.color && size == b.size && !std::strcmp(text, b.text);
}
void FooterPanel::create(lv_obj_t *parent, int channel) {
    object_ = lv_obj_create(parent);
    lv_obj_remove_style_all(object_);
    lv_obj_set_scrollable(object_, false);
    lv_obj_set_pos(object_, Layout::panel(channel).x, Layout::PanelY);
    lv_obj_set_size(object_, Layout::PanelWidth, Layout::PanelHeight);
    lv_obj_add_event_cb(object_, draw, LV_EVENT_DRAW_MAIN, this);
    count_ = 0; lit_ = 0; next_ = shown_ = {};
    lv_obj_set_hidden(object_, true);
}
void FooterPanel::begin(uint32_t fill, uint32_t frame) {
    next_.fill = fill; next_.frame = frame; next_.lit = 0; next_.railColor = 0;
    count_ = 0;
}
void FooterPanel::add(int x, const char *text, uint32_t color, unsigned size) {
    if (count_ >= Texts || !text || !*text) return;
    auto &t = texts_[count_++];
    std::snprintf(t.text, sizeof t.text, "%s", text);
    t.size = uint8_t(size);
    t.x = int16_t(x); t.width = int16_t(Elements::textWidth(t.text, size)); t.color = color;
}
void FooterPanel::rail(int lit, uint32_t color) {
    next_.lit = std::clamp(lit, 0, Layout::PanelWidth - 2 * Layout::Border);
    next_.railColor = color;
}
void FooterPanel::commit(bool visible) {
    next_.visible = visible; next_.count = count_;
    lit_ = visible ? next_.lit : 0;
    bool same = next_.fill == shown_.fill && next_.frame == shown_.frame && next_.lit == shown_.lit && next_.railColor == shown_.railColor &&
                next_.count == shown_.count && next_.visible == shown_.visible;
    for (unsigned i = 0; same && i < count_; ++i) same = texts_[i] == drawn_[i];
    if (same) return;
    std::copy(texts_, texts_ + count_, drawn_);
    shown_ = next_;
    if (lv_obj_is_hidden(object_) == visible) lv_obj_set_hidden(object_, !visible);
    lv_obj_invalidate(object_);
}
// The fill and frame, the rail over the bottom frame row, then each text in its box (the panel's
// text column from Layout::Inset, the footer's text row).
void FooterPanel::draw(lv_event_t *event) {
    const auto &panel = *static_cast<FooterPanel *>(lv_event_get_user_data(event));
    auto *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(panel.object_, &area);
    const auto &state = panel.shown_;
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init(&box);
    box.bg_color = lv_color_hex(state.fill); box.bg_opa = LV_OPA_COVER;
    box.border_color = lv_color_hex(state.frame); box.border_width = Layout::Border; box.border_opa = LV_OPA_COVER;
    box.radius = Radius;
    lv_draw_rect(layer, &box, &area);
    if (state.lit > 0) {
        lv_draw_rect_dsc_t rail;
        lv_draw_rect_dsc_init(&rail);
        rail.bg_color = lv_color_hex(state.railColor); rail.bg_opa = LV_OPA_COVER;
        const lv_area_t lit = {area.x1 + Layout::Border, area.y2, area.x1 + Layout::Border + state.lit - 1, area.y2};
        lv_draw_rect(layer, &rail, &lit);
    }
    const int left = area.x1 + Layout::Inset;
    for (unsigned i = 0; i < state.count; ++i) {
        const auto &text = panel.drawn_[i];
        lv_draw_label_dsc_t label;
        lv_draw_label_dsc_init(&label);
        label.font = Elements::font(text.size); // The font ReadoutText measured it in.
        const int line = lv_font_get_line_height(label.font), top = area.y1 - Layout::PanelY + (Layout::Height - line) / 2;
        label.color = lv_color_hex(text.color);
        label.text = text.text;
        label.text_local = 1;
        label.flag = LV_TEXT_FLAG_EXPAND; // Measured to fit: never wrapped.
        const lv_area_t box = {left + text.x, top, left + text.x + std::max<int>(1, text.width) - 1, top + line - 1};
        lv_draw_label(layer, &label, &box);
    }
}
} // namespace openosc::ui
