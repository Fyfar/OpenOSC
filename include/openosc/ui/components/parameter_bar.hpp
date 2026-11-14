#pragma once
#include "openosc/ui/components/elements.hpp"

namespace openosc::ui {
class ParameterBar {
public:
    static constexpr unsigned Count = 4;
    lv_obj_t *create(lv_obj_t *parent);
    void palette(const Theme &theme, const ElementStyle &style);
    // The cards' text after palette() (the DMM's Card text): its font and colour. `selected`: the
    // selected card's text takes it too (an explicit colour; the frame keeps the highlight); else
    // the selected text stays in the highlight. palette() on the next mode change restores the theme's.
    void text(const lv_font_t *font, uint32_t color, bool selected);
    void item(unsigned index, const char *title, const char *value);
    void select(unsigned index);
    lv_obj_t *card(unsigned index) const { return index < Count ? cards_[index] : nullptr; }
    unsigned selected() const { return selected_; }
    void firstWide(bool wide) { if (firstWide_ != wide) { firstWide_ = wide; arrange(); } } // Room for two scales.

private:
    static void resized(lv_event_t *event);
    void arrange();
    lv_obj_t *root_ = nullptr;
    lv_obj_t *cards_[Count]{}, *titles_[Count]{}, *values_[Count]{};
    unsigned selected_ = Count;
    bool firstWide_ = false, selectedText_ = false; // The selected card's text in foreground_.
    uint32_t background_ = 0, border_ = 0, highlight_ = 0, foreground_ = 0;
};
} // namespace openosc::ui
