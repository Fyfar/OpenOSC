#pragma once
#include "openosc/ui/components/scope_footer_layout.hpp"
#include "lvgl.h"
#include <cstdint>

namespace openosc::ui {
// One channel's panel of the Scope footer, drawn by one LVGL object (no label per text: the
// LVGL heap is shared with every screen): a faint fill and a thin frame, the rail along the
// bottom frame row, and up to Texts single-line texts, each in its own colour and font size
// (Elements::font: IBM Plex Sans 12, or its condensed 11) at its own place, centred on the row.
// The owner sets a whole state, then commit() invalidates the panel
// only when it differs from what is on screen.
class FooterPanel {
public:
    static constexpr unsigned Texts = 2 * ScopeFooterLayout::PerChannel; // A tag and a value per readout.
    struct Text {
        char text[24] = "";
        int16_t x = 0, width = 0; // From the left of the panel's text column; width is the text's own.
        uint32_t color = 0;
        uint8_t size = 12;
        bool operator==(const Text &b) const;
    };
    void create(lv_obj_t *parent, int channel);
    void begin(uint32_t fill, uint32_t frame); // A new state: no texts, no rail.
    void add(int x, const char *text, uint32_t color, unsigned size = 12);
    void rail(int lit, uint32_t color);
    void commit(bool visible);
    unsigned count() const { return count_; }
    const Text &text(unsigned index) const { return texts_[index]; }
    int lit() const { return lit_; }
    lv_obj_t *object() const { return object_; }

private:
    struct State {
        uint32_t fill = 0, frame = 0, railColor = 0;
        int lit = 0;
        unsigned count = 0;
        bool visible = false;
    };
    static void draw(lv_event_t *event);
    lv_obj_t *object_ = nullptr;
    Text texts_[Texts], drawn_[Texts];
    unsigned count_ = 0;
    int lit_ = 0;
    State next_, shown_;
};
} // namespace openosc::ui
