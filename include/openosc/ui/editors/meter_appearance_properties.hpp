#pragma once
#include "openosc/settings/meter_appearance.hpp"
#include "openosc/settings/theme.hpp"
#include "openosc/ui/editors/color_rows.hpp"
#include "openosc/ui/editors/property_list.hpp"

namespace openosc::ui {
// DMM Measurement > Appearance: the base (palette, reading face), then one element at a time with
// its own properties. Rows: Preset, Base colors, Element, Color (ColorRows: Default, the named
// colours, Custom RGB; then Red, Green, Blue for Custom), Font (every text element: IBM or
// Mono; the reading: IBM, Mono or Segment, Default being the Digits face), Style (only for one with
// a choice of line weights: arc, ticks, needle), Reset element, Reset all. Every choice is taken on MENU (PropertyNavigator,
// Changed) and the owner applies and saves it; F4 leaves a list unchanged. Element only selects
// what the rows below edit; it is no setting. Preset Hybrid writes the photo's overrides once; they
// stay editable one by one (the row reads Custom as soon as one differs). Colour options and rows
// show their samples; Default (and the Base and Element samples) need `theme` and `parameters`
// (the DMM cards' highlight, as MeterStyles::look takes them): without a theme, no sample there.
class MeterAppearanceProperties final : public PropertyList {
public:
    enum Row : uint8_t { Preset, Base, Element, Color, Red, Green, Blue, Font, Style, ResetElement, ResetAll, Rows };
    MeterAppearanceProperties(unsigned palette = 0, unsigned face = 0, const MeterAppearance &appearance = MeterAppearance(),
                              MeterRole element = MeterRole::Reading, const Theme *theme = nullptr, uint32_t parameters = 0)
        : palette_(palette), face_(face), appearance_(appearance), element_(element), theme_(theme), parameters_(parameters) {}
    unsigned palette() const { return palette_; }
    unsigned face() const { return face_; }
    const MeterAppearance &appearance() const { return appearance_; }
    MeterRole element() const { return element_; }
    // The row shown at `index` (Red/Green/Blue only for Custom RGB, Font and Style only where the
    // element has them).
    Row at(unsigned index) const;
    unsigned index(Row row) const; // Its index, or count() if not shown.
    bool hybrid() const;
    // Reset element / Reset all (the Action rows): back to the base.
    void reset(Row row);

    const char *title() const override { return "APPEARANCE"; }
    unsigned count() const override;
    PropertyKind kind(unsigned index) const override;
    void name(unsigned index, char *output, size_t capacity) const override;
    unsigned options(unsigned index) const override;
    unsigned option(unsigned index) const override;
    void optionName(unsigned index, unsigned option, char *output, size_t capacity) const override;
    bool choose(unsigned index, unsigned option) override;
    void value(unsigned index, char *output, size_t capacity) const override;
    void adjust(unsigned index, int delta) override;
    int coarseStep(unsigned index) const override;
    int32_t number(unsigned index) const override;
    void setNumber(unsigned index, int32_t value) override;
    bool optionSwatch(unsigned index, unsigned option, uint32_t &rgb) const override;
    bool valueSwatch(unsigned index, uint32_t &rgb) const override;

private:
    const MeterRoleInfo &info() const { return MeterAppearanceCatalog::role(element_); }
    bool shown(Row row) const;
    uint32_t inherited(MeterRole role) const; // The base colour of a role, or ColorRows::Unknown.
    ColorValue color() const { return appearance_[element_].colorValue(); }
    unsigned palette_, face_;
    MeterAppearance appearance_;
    MeterRole element_;
    const Theme *theme_;
    uint32_t parameters_;
};
} // namespace openosc::ui
