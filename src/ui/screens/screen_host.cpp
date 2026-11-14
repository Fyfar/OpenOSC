#include "openosc/ui/screens/screen_host.hpp"
#include "openosc/ui/components/meter_reading.hpp"
#include <algorithm>

namespace openosc::ui {
void ScreenHost::status(const char *text, StatusTone tone) {
    elements_.text(ElementId::Status, text);
    lv_obj_set_width(elements_.get(ElementId::Status), LV_SIZE_CONTENT); // The theme re-applies its box on every activation.
    statusWidth_ = std::min(Elements::textWidth(text) + 2 * StatusBarLayout::PillPadding, StatusBarLayout::MaximumStatus);
    statusRight_ = statusX_ + statusWidth_;
    if (tone == tone_) return;
    tone_ = tone;
    static constexpr struct { uint32_t background, text; } colors[] = {
        {0x334155, 0xe2e8f0}, {0x22c55e, 0x052e16}, {0xfbbf24, 0x1c1400}, {0xef4444, 0xffffff}};
    auto *pill = elements_.get(ElementId::Status);
    lv_obj_set_style_bg_color(pill, lv_color_hex(colors[unsigned(tone)].background), 0);
    lv_obj_set_style_text_color(pill, lv_color_hex(colors[unsigned(tone)].text), 0);
}
void ScreenHost::statusX(int x) {
    if (x == statusX_) return;
    statusX_ = x; statusRight_ = statusX_ + statusWidth_;
    if (root_) placeStatus();
}
void ScreenHost::placeStatus() { lv_obj_set_pos(elements_.get(ElementId::Status), statusX_, 1); }
void ScreenHost::build(lv_obj_t *parent, const Theme &theme) {
    clear();
    theme_ = &theme;
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, 320, 240);
    lv_obj_set_scrollable(root_, false);
    lv_obj_add_event_cb(root_, [](lv_event_t *event) {
        static_cast<ScreenHost *>(lv_event_get_user_data(event))->root_ = nullptr;
    }, LV_EVENT_DELETE, this);
    meterPanel_ = lv_obj_create(root_); // First: below everything the theme places.
    lv_obj_remove_style_all(meterPanel_);
    lv_obj_set_scrollable(meterPanel_, false);
    lv_obj_set_style_bg_opa(meterPanel_, LV_OPA_COVER, 0);
    for (unsigned i = 0; i < ElementCount; ++i) {
        const auto id = static_cast<ElementId>(i);
        lv_obj_t *object;
        switch (id) {
        case ElementId::Waveform: object = waveform_.create(root_); break;
        case ElementId::Parameters: object = parameters_.create(root_); break;
        case ElementId::Preview: object = preview_.create(root_); break;
        case ElementId::Icon: object = lv_image_create(root_); break;
        default: object = Elements::label(root_, ""); break;
        }
        elements_.bind(id, object);
    }
    chrome_ = lv_obj_create(root_);
    lv_obj_remove_style_all(chrome_);
    lv_obj_set_size(chrome_, StatusBarLayout::Width, StatusBarLayout::Height);
    lv_obj_set_scrollable(chrome_, false);
    lv_obj_set_style_bg_opa(chrome_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(chrome_, lv_color_hex(theme.background), 0);
    auto *pill = elements_.get(ElementId::Status);
    lv_obj_set_parent(pill, chrome_);
    lv_obj_set_style_pad_hor(pill, StatusBarLayout::PillPadding, 0);
    lv_obj_set_style_max_width(pill, StatusBarLayout::MaximumStatus, 0); // Clipped before the FPS column.
    lv_obj_set_style_pad_top(pill, 2, 0); // Centres the capitals on the bar's axis, y=10.
    lv_obj_set_style_radius(pill, 2, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    tone_ = StatusTone(255);
    meterSummary_.create(elements_.get(ElementId::Measurements));
    scopeFooter_.create(elements_.get(ElementId::Measurements), theme);
    meterTrend_.create(elements_.get(ElementId::Measurements), theme);
    dial_.create(root_);         // The dial first: the function sign is drawn over its empty corner.
    functionIcon_.create(root_);
    meterCards_.create(parameters_);
    waveform_.inlineScales(theme.layout == LayoutPreset::Legacy);
    waveform_.colors(theme);
    waveform_.configure(theme.plot);
    waveform_.clear();
}
void ScreenHost::activate(MeasurementMode mode) {
    tone_ = StatusTone(255); // Applying the theme below resets the pill colours.
    overlay_ = false;
    meterTrend_.visible(false);
    meterSummary_.visible(mode == MeasurementMode::Multimeter); // Every preset has the same DMM body.
    scopeFooter_.visible(mode == MeasurementMode::Oscilloscope && theme_->layout != LayoutPreset::Legacy);
    // The DMM shows these itself (MeterScreen); every other screen has none.
    lv_obj_set_hidden(meterPanel_, true);
    functionIcon_.visible(false);
    dial_.visible(false);
    meterCards_.visible(false);
    elements_.apply(theme_->elements[unsigned(mode)]);
    MeterReading::release(elements_.get(ElementId::Reading)); // The DMM sets its sign column again on activation.
    // Status is part of the instrument shell, never a hideable body element.
    auto *status = elements_.get(ElementId::Status);
    lv_obj_set_size(status, StatusBarLayout::MaximumStatus, 19);
    placeStatus();
    lv_obj_set_style_text_font(status, Elements::font(12), 0);
    lv_obj_set_hidden(status, false);
    lv_obj_move_foreground(chrome_);
    parameters_.firstWide(mode == MeasurementMode::Oscilloscope);
    parameters_.palette(*theme_, theme_->elements[unsigned(mode)][unsigned(ElementId::Parameters)]);
    lv_obj_set_hidden(root_, false);
}
void ScreenHost::overlay(bool parameters) {
    overlay_ = true;
    lv_obj_set_hidden(meterPanel_, true); // A menu over the DMM: no dial work underneath it.
    functionIcon_.visible(false);
    dial_.visible(false);
    for (unsigned i = 0; i < ElementCount; ++i) {
        const auto id = ElementId(i);
        lv_obj_set_hidden(elements_.get(id), id != ElementId::Status && !(parameters && id == ElementId::Parameters));
    }
    if (parameters) {
        auto *bar = elements_.get(ElementId::Parameters);
        lv_obj_set_pos(bar, 8, 22);
        lv_obj_set_size(bar, 304, 32);
    }
}
} // namespace openosc::ui
