#pragma once
#include "openosc/settings/meter_appearance.hpp"
#include "openosc/ui/editors/property_list.hpp"

namespace openosc::ui {
// DMM Measurement > Appearance: the base (palette, reading face), then one element at a time with
// its own properties. Rows: Preset, Base colors, Element, Color, Font (every text element: IBM or
// Mono; the reading: IBM, Mono or Segment, Default being the Digits face), Style (only for one with
// a choice of line weights: arc, ticks, needle), Reset element, Reset all. Every choice is taken on MENU (PropertyNavigator,
// Changed) and the owner applies and saves it; F4 leaves a list unchanged. Element only selects
// what the rows below edit; it is no setting. Preset Hybrid writes the photo's overrides once; they
// stay editable one by one (the row reads Custom as soon as one differs).
class MeterAppearanceProperties final : public PropertyList {
public:
    enum Row : uint8_t { Preset, Base, Element, Color, Font, Style, ResetElement, ResetAll, Rows };
    MeterAppearanceProperties(unsigned palette = 0, unsigned face = 0, const MeterAppearance &appearance = MeterAppearance(), MeterRole element = MeterRole::Reading)
        : palette_(palette), face_(face), appearance_(appearance), element_(element) {}
    unsigned palette() const { return palette_; }
    unsigned face() const { return face_; }
    const MeterAppearance &appearance() const { return appearance_; }
    MeterRole element() const { return element_; }
    // The row shown at `index` (Font and Style only where the element has them).
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

private:
    const MeterRoleInfo &info() const { return MeterAppearanceCatalog::role(element_); }
    unsigned palette_, face_;
    MeterAppearance appearance_;
    MeterRole element_;
};
} // namespace openosc::ui
