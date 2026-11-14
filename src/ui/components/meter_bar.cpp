#include "openosc/ui/components/meter_bar.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace openosc::ui {
void MeterBar::draw(lv_event_t *event) {
    auto &self = *static_cast<MeterBar *>(lv_event_get_user_data(event));
    auto *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(self.bar_, &area);
    lv_draw_rect_dsc_t on, off;
    lv_draw_rect_dsc_init(&on); lv_draw_rect_dsc_init(&off);
    on.bg_color = lv_color_hex(self.color_); off.bg_color = lv_color_hex(self.off_);
    for (int i = 0; i < Segments; ++i) {
        lv_area_t segment = {lv_coord_t(area.x1 + i * Pitch), area.y1, lv_coord_t(area.x1 + i * Pitch + SegmentWidth - 1), area.y2};
        lv_draw_rect(layer, i < self.lit_ ? &on : &off, &segment);
    }
}
void MeterBar::create(lv_obj_t *parent) {
    initialized_ = false; lit_ = 0;
    sign_ = Elements::label(parent, "", 0, 63);
    lv_obj_set_size(sign_, BarX, 14);
    bar_ = lv_obj_create(parent);
    lv_obj_remove_style_all(bar_);
    lv_obj_set_pos(bar_, BarX, 65);
    lv_obj_set_size(bar_, Segments * Pitch - 1, Height);
    lv_obj_set_scrollable(bar_, false);
    lv_obj_add_event_cb(bar_, draw, LV_EVENT_DRAW_MAIN, this);
    for (int i = 0; i <= 5; ++i) { // Ticks every 5000 counts along the scale.
        char text[4]; std::snprintf(text, sizeof text, "%d", i * 5);
        const int tick = BarX + i * Segments / 5 * Pitch;
        auto *label = ticks_[i] = Elements::label(parent, text, i == 5 ? tick - 16 : i ? tick - 8 : tick, 75);
        lv_obj_set_size(label, 16, 13);
        lv_obj_set_style_text_align(label, i == 5 ? LV_TEXT_ALIGN_RIGHT : i ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
    }
    state_ = Elements::label(parent, "NO DATA", BarX + 118, 75); // Between ticks 10 and 15.
    lv_obj_set_size(state_, 52, 13);
    lv_obj_set_style_text_align(state_, LV_TEXT_ALIGN_CENTER, 0);
    palette({0, 0, muted_, 0, off_, 0, overload_});
}
void MeterBar::palette(const MeterPalette &palette) {
    const bool repaint = palette.line != off_ || palette.alert != overload_;
    off_ = palette.line; overload_ = palette.alert; muted_ = palette.muted;
    Elements::textColor(sign_, muted_);
    for (auto *tick : ticks_) Elements::textColor(tick, muted_);
    Elements::textColor(state_, overload_);
    if (repaint) { initialized_ = false; lv_obj_invalidate(bar_); } // The OL colour is in color_.
}
void MeterBar::visible(bool value) {
    for (auto *object : {bar_, state_, sign_}) lv_obj_set_hidden(object, !value);
    for (auto *tick : ticks_) lv_obj_set_hidden(tick, !value);
}
void MeterBar::update(const dmm::Result &result, DataStatus health, uint32_t accent,
                      const dmm::RelativeReading *relative) {
    const auto value = dmm::BarReading::from(result,health,relative);
    const bool over = value.status == DataStatus::Overload;
    const auto color = over ? overload_ : value.status == DataStatus::Ok ? accent : 0x64748b;
    const int magnitude = std::abs(value.counts);
    const int lit = over ? Segments : std::min(Segments, (magnitude * Segments + dmm::BarReading::FullScale - 1) / dmm::BarReading::FullScale);
    const bool negative = value.counts < 0;
    if (initialized_ && lit == lit_ && negative == negative_ && status_ == value.status && color_ == color) return;
    const bool repaint = !initialized_ || lit != lit_ || color_ != color;
    if (!initialized_ || negative != negative_) lv_label_set_text(sign_, negative ? "-" : "");
    if (!initialized_ || status_ != value.status)
        lv_label_set_text(state_, value.status == DataStatus::Ok ? "" : over ? "OL" :
            value.status == DataStatus::Stale ? "STALE" : value.status == DataStatus::NoData ? "NO DATA" : "INVALID");
    initialized_ = true; lit_ = lit; negative_ = negative; status_ = value.status; color_ = color;
    if (repaint) lv_obj_invalidate(bar_);
}
}
