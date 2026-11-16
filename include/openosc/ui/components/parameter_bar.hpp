#pragma once
#include "openosc/ui/components/elements.hpp"
#include "openosc/ui/components/soft_keys.hpp"

namespace openosc::ui {
// The four soft-key zones over F1..F4: equal widths (ScreenLayout::ButtonWidth on its 316 px bar),
// each a title row (the action, or its context) and a value row. A text too wide for IBM Plex 12
// takes the condensed 11 px face, so it stays whole; a marked title (the Scope's Next scales) is
// drawn in the highlight colour. Two layers: the instrument screen's cards (item, select), kept as
// last set, and a context's (context: a panel, menu, confirmation, calibration, Remote) drawn over
// them until release(); the screen's background updates never show through a context.
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
    // A context's keys over the screen's: inert keys muted, never accented; the screen's own drawings
    // on the cards (the DMM's function signs) hidden. release(): the screen's cards as last set.
    void context(const SoftKeys &keys);
    void release();
    // The screen's own drawing on a card shown or hidden (the DMM's signs): under a context it stays
    // hidden and appears with release(), whenever the screen asks.
    void decorate(lv_obj_t *child, bool visible);
    bool contextShown() const { return context_; }
    bool live(unsigned index) const { return index < Count && live_[index]; } // Tests: the key acts here.
    lv_obj_t *card(unsigned index) const { return index < Count ? cards_[index] : nullptr; }
    unsigned selected() const { return selected_; } // On show (the context's while one is shown).
    lv_obj_t *title(unsigned index) const { return index < Count ? titles_[index] : nullptr; } // Tests.
    lv_obj_t *value(unsigned index) const { return index < Count ? values_[index] : nullptr; }

private:
    static constexpr unsigned TitleBytes = 24, ValueBytes = 40, Decorations = 4;
    struct Card { char title[TitleBytes] = "", value[ValueBytes] = ""; bool marked = false; };
    static void resized(lv_event_t *event);
    void show(unsigned index, const char *title, const char *value); // Texts on the labels.
    void paint();                         // Borders and colours of the layer on show.
    void decorations(bool visible);       // The screen's own children of the cards.
    void arrange();
    void fit(unsigned index); // The title's and the value's face: 12, or condensed 11 when 12 does not fit.
    lv_obj_t *root_ = nullptr;
    lv_obj_t *cards_[Count]{}, *titles_[Count]{}, *values_[Count]{};
    unsigned selected_ = Count, screenSelected_ = Count;
    Card screen_[Count];                  // The screen's cards, as last set (shown when no context is).
    bool live_[Count]{true, true, true, true}, context_ = false;
    lv_obj_t *hidden_[Count][Decorations]{}; // What context() hid, shown again by release().
    bool selectedText_ = false, customFont_ = false; // The selected card's text in foreground_; text() font in force.
    int widths_[Count]{};
    uint32_t background_ = 0, border_ = 0, highlight_ = 0, foreground_ = 0, muted_ = 0;
};
} // namespace openosc::ui
