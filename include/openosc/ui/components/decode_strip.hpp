#pragma once
#include "openosc/decode/result.hpp"
#include "openosc/settings/theme.hpp"
#include "openosc/ui/scope/viewport.hpp"
#include "lvgl.h"

namespace openosc::ui {
// Wave + annotations (plan §4.1): the decoded events under the trace, along the plot's bottom
// rows; one self-drawn LVGL object as wide as the plot. An event is a frame from its first to its
// last point with its label (TableText::label: an error's code in the warning colour), placed by the
// trace's own viewport (Viewport::sampleX: pan, zoom, a window past
// either end of the record), clipped to the plot; its text when at least TextWidth wide, else a
// tick at its start (texts never overlap). The caller passes only a result of the record the
// trace shows (DecodeJob::shows), or none.
class DecodeStrip {
public:
    static constexpr int TextWidth = 18, CharWidth = 7; // openosc_mono_12: 115/16 px a glyph, drawn at 7.
    void create(lv_obj_t *parent);
    void colors(const Theme &theme);
    void place(int x, int y, int width, int height);
    void hide();
    // `sets`: the result's owner; LVGL's later draw checks the set is still its published one at this revision.
    void show(const decoding::ResultSet *set, const decoding::ResultSets *sets, uint32_t revision, const plot::Viewport &viewport);
    bool visible() const { return object_ && !lv_obj_is_hidden(object_); }
    lv_obj_t *object() const { return object_; }
    // The columns [x1, x2] an event [first, last) covers in the plot (`width` columns); false when
    // none of it is in view.
    static bool columns(const plot::Viewport &viewport, uint32_t first, uint32_t last, int &x1, int &x2);

private:
    static void draw(lv_event_t *event);
    lv_obj_t *object_ = nullptr;
    const decoding::ResultSet *set_ = nullptr;
    const decoding::ResultSets *sets_ = nullptr;
    uint32_t revision_ = 0xFFFFFFFFu;
    plot::Viewport viewport_;
    uint32_t foreground_ = 0xFFFFFF, background_ = 0, accent_ = 0x808080, warning_ = 0xfbbf24;
};
} // namespace openosc::ui
