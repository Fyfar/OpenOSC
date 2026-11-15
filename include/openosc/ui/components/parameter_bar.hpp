#pragma once
#include "openosc/ui/components/elements.hpp"

namespace openosc::ui {
// The four soft-key zones over F1..F4: equal widths (ScreenLayout::ButtonWidth on its 316 px bar),
// each a title row (the action, or its context) and a value row. A text too wide for IBM Plex 12
// takes the condensed 11 px face, so it stays whole; a marked title (the Scope's Next scales) is
// drawn in the highlight colour.
class ParameterBar {
public:
    static constexpr unsigned Count = 4;
    lv_obj_t *create(lv_obj_t *parent);
    void palette(const Theme &theme, const ElementStyle &style);
    // The cards' text after palette() (the DMM's Card text): its font and colour. `selected`: the
    // selected card's text takes it too (an explicit colour; the frame keeps the highlight); else
    // the selected text stays in the highlight. palette() on the next mode change restores the theme's.
    void text(const lv_font_t *font, uint32_t color, bool selected);
    void item(unsigned index, const char *title, const char *value, bool marked = false);
    void select(unsigned index);
    lv_obj_t *card(unsigned index) const { return index < Count ? cards_[index] : nullptr; }
    unsigned selected() const { return selected_; }
    lv_obj_t *title(unsigned index) const { return index < Count ? titles_[index] : nullptr; } // Tests.
    lv_obj_t *value(unsigned index) const { return index < Count ? values_[index] : nullptr; }

private:
    static void resized(lv_event_t *event);
    void arrange();
    void fit(unsigned index); // The title's and the value's face: 12, or condensed 11 when 12 does not fit.
    lv_obj_t *root_ = nullptr;
    lv_obj_t *cards_[Count]{}, *titles_[Count]{}, *values_[Count]{};
    unsigned selected_ = Count;
    bool selectedText_ = false, customFont_ = false; // The selected card's text in foreground_; text() font in force.
    bool marked_[Count]{};
    int widths_[Count]{};
    uint32_t background_ = 0, border_ = 0, highlight_ = 0, foreground_ = 0, muted_ = 0;
};
} // namespace openosc::ui
