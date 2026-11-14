#pragma once
#include "openosc/ui/components/elements.hpp"
#include "openosc/measurement/dmm/bar.hpp"
#include "openosc/ui/components/meter_style.hpp"

namespace openosc::ui {
// Segmented bar in the style of a handheld DMM: zero at the left, magnitude to the right, a
// separate polarity sign and a fixed 0-25 scale (thousands of counts). Redrawn on every packet.
class MeterBar {
public:
    void create(lv_obj_t *parent);
    void update(const dmm::Result &, DataStatus, uint32_t accent,
                const dmm::RelativeReading *relative = nullptr);
    void palette(const MeterPalette &palette); // Unlit segments, scale text and the OL colour.
    void visible(bool value);                   // The analog dial replaces the bar.
private:
    static constexpr int Segments = 48, Pitch = 6, SegmentWidth = 5, Height = 9, BarX = 8;
    static void draw(lv_event_t *event);
    lv_obj_t *bar_ = nullptr, *state_ = nullptr, *sign_ = nullptr, *ticks_[6]{};
    uint32_t off_ = 0x243449, overload_ = 0xf87171, muted_ = 0x94a3b8;
    int lit_ = 0;
    uint32_t color_ = 0;
    DataStatus status_ = DataStatus::NoData;
    bool negative_ = false, initialized_ = false;
};
}
