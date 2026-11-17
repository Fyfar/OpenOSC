#pragma once
#include "openosc/decode/table_text.hpp"
#include "openosc/settings/theme.hpp"
#include "openosc/ui/screens/screen_layout.hpp"
#include "lvgl.h"

namespace openosc::ui {
// The decoder's table (plan §4.2, §4.3): one self-drawn LVGL object, no label per cell. From the top
// (DecoderTableRows): the info row (state, warnings, counters) when the place has one, the header, then
// `rows` events from the one that keeps the selection in view (the selected row marked '>' and
// inverted), or in Detail one event line by line. Without an info row (Split) the header is the one
// title row: the time column's name (the record's origin and unit, TableText's header cell) then the
// info text, cut to a row with '~'; the other columns' names are left out.
// It draws the published result it is shown through TableText at draw time, and only while that set
// is still the owner's published one at the same revision (a publish or clear since show(): nothing
// drawn until the next show, never another record's events) and keeps no text of its own but the info row. A change of the info
// row redraws only that row; the same content draws nothing.
class DecodeTable {
public:
    struct Content {
        const decoding::ResultSet *set = nullptr; // The result on show (nullptr: none yet).
        const decoding::ResultSets *sets = nullptr; // Its owner: checked again when LVGL draws.
        uint32_t revision = 0;                    // ResultSets::revision of it.
        unsigned selected = 0;
        const char *empty = "";                   // A static text: why there is no event.
        const decoding::Settings *settings = nullptr; // Detail's thresholds and formats (the job's, in force).
        bool detail = false;
        unsigned detailLine = 0;                  // Detail: its first line shown.
    };
    void create(lv_obj_t *parent);
    void colors(const Theme &theme);
    void place(int y, const DecoderTableRows &rows);
    void hide();
    void show(const Content &content, const char *info);
    bool visible() const { return object_ && !lv_obj_is_hidden(object_); }
    unsigned rows() const { return rows_.events; }
    const DecoderTableRows &layout() const { return rows_; }
    unsigned first() const { return first_; } // The event in the first row.
    const char *info() const { return info_; }
    lv_obj_t *object() const { return object_; }
    static constexpr int TextX = 2; // Plan §4.2: 2 px either side of a full row.
    // Split's title row from TableText's header line and the info text: at most a row (43 characters).
    static void title(const decoding::TableText::Line &header, const char *info, decoding::TableText::Line &out);

private:
    static void draw(lv_event_t *event);
    lv_obj_t *object_ = nullptr;
    Content content_;
    decoding::TableText::Line info_{};
    unsigned first_ = 0;
    DecoderTableRows rows_;
    bool drawn_ = false; // The content above is on screen (a new place or theme: drawn again).
    uint32_t foreground_ = 0xFFFFFF, background_ = 0, muted_ = 0x808080, panel_ = 0, warning_ = 0xfbbf24;
};
} // namespace openosc::ui
