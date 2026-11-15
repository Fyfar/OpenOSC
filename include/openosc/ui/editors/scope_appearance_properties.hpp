#pragma once
#include "openosc/settings/scope_appearance.hpp"
#include "openosc/settings/theme.hpp"
#include "openosc/ui/editors/color_rows.hpp"
#include "openosc/ui/editors/property_list.hpp"

namespace openosc::ui {
// Scope Colors: Scheme, Element, then the element's ColorRows (Color; Red, Green, Blue for Custom
// RGB), Reset element, Reset all. Every colour option has its sample (optionSwatch), every colour
// row its value's (valueSwatch); Default shows the scheme's own colour of the element (Trigger:
// "Source", the source channel's; Tag text: "Auto", the contrast of each tag: no sample). Element
// only selects what the rows below edit; it is no setting. The owner applies appearance() on each
// Changed or Activate (a repaint, nothing else) and saves it; F4 leaves a list or a number as it
// was. `theme` is the base theme (Theme JSON), not the effective one, and must outlive the list.
class ScopeAppearanceProperties final : public PropertyList {
public:
    enum Row : uint8_t { Scheme, Element, Source, Red, Green, Blue, ResetElement, ResetAll, Rows };
    ScopeAppearanceProperties(const Theme &theme, const ScopeAppearance &appearance = ScopeAppearance(), ScopeRole element = ScopeRole::Channel1)
        : theme_(&theme), appearance_(appearance), element_(element) {}
    const ScopeAppearance &appearance() const { return appearance_; }
    ScopeRole element() const { return element_; }
    Row at(unsigned index) const;  // Red/Green/Blue only for Custom RGB.
    unsigned index(Row row) const; // count() if not shown.
    void reset(Row row);           // Reset element / Reset all (Reset all keeps the scheme).

    const char *title() const override { return "COLORS"; }
    unsigned count() const override;
    PropertyKind kind(unsigned index) const override;
    void name(unsigned index, char *output, size_t capacity) const override;
    void value(unsigned index, char *output, size_t capacity) const override;
    unsigned options(unsigned index) const override;
    unsigned option(unsigned index) const override;
    void optionName(unsigned index, unsigned option, char *output, size_t capacity) const override;
    bool choose(unsigned index, unsigned option) override;
    void adjust(unsigned index, int delta) override;
    int coarseStep(unsigned index) const override;
    int32_t number(unsigned index) const override;
    void setNumber(unsigned index, int32_t value) override;
    bool optionSwatch(unsigned index, unsigned option, uint32_t &rgb) const override;
    bool valueSwatch(unsigned index, uint32_t &rgb) const override;

private:
    uint32_t inherited(ScopeRole role) const; // The role's colour without its override, or ColorRows::Unknown.
    uint32_t start() const;                   // Custom RGB's first colour where Default has none.
    const Theme *theme_;
    ScopeAppearance appearance_;
    ScopeRole element_;
};
} // namespace openosc::ui
