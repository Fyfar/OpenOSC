#include "openosc/ui/editors/meter_appearance_properties.hpp"
#include "openosc/ui/components/meter_style.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
constexpr const char *presets[] = {"Custom", "Hybrid"};
// Choice option 0 is the inherited value; 1.. the role's swatches, fonts or styles.
unsigned choice(uint8_t value) { return value == MeterRoleStyle::Inherit ? 0 : value + 1U; }
uint8_t stored(unsigned option) { return option ? uint8_t(option - 1) : MeterRoleStyle::Inherit; }
} // namespace
MeterAppearanceProperties::Row MeterAppearanceProperties::at(unsigned index) const {
    unsigned shown = 0;
    for (unsigned r = 0; r < Rows; ++r) {
        const auto row = Row(r);
        if ((row == Font && !info().fonts) || (row == Style && !info().styles)) continue;
        if (shown++ == index) return row;
    }
    return Rows;
}
unsigned MeterAppearanceProperties::index(Row row) const {
    for (unsigned i = 0; i < count(); ++i) if (at(i) == row) return i;
    return count();
}
unsigned MeterAppearanceProperties::count() const { return Rows - (info().fonts ? 0 : 1) - (info().styles ? 0 : 1); }
bool MeterAppearanceProperties::hybrid() const {
    return palette_ == MeterAppearanceCatalog::HybridPalette && face_ == MeterAppearanceCatalog::HybridFace &&
           appearance_ == MeterAppearanceCatalog::hybrid();
}
PropertyKind MeterAppearanceProperties::kind(unsigned index) const {
    const auto row = at(index);
    return row == ResetElement || row == ResetAll ? PropertyKind::Action : PropertyKind::Choice;
}
void MeterAppearanceProperties::name(unsigned index, char *output, size_t capacity) const {
    static constexpr const char *names[Rows] = {"Preset", "Base colors", "Element", "Color", "Font", "Style", "Reset element", "Reset all"};
    const auto row = at(index);
    std::snprintf(output, capacity, "%s", row < Rows ? names[row] : "");
}
unsigned MeterAppearanceProperties::options(unsigned index) const {
    switch (at(index)) {
    case Preset: return 2;
    case Base: return MeterStyles::Schemes;
    case Element: return MeterAppearance::Roles;
    case Color: return 1 + MeterAppearanceCatalog::Swatches;
    case Font: return 1 + info().fonts; // Default (the reading: its Digits face; text: IBM), then the typefaces.
    case Style: return 1 + info().styles;
    default: return 0;
    }
}
unsigned MeterAppearanceProperties::option(unsigned index) const {
    const auto &style = appearance_[element_];
    switch (at(index)) {
    case Preset: return hybrid() ? 1 : 0;
    case Base: return palette_;
    case Element: return unsigned(element_);
    case Color: return choice(style.color);
    case Font: return choice(style.font);
    case Style: return choice(style.style);
    default: return 0;
    }
}
void MeterAppearanceProperties::optionName(unsigned index, unsigned option, char *output, size_t capacity) const {
    const auto row = at(index);
    const char *text = "";
    switch (row) {
    case Preset: text = presets[option < 2 ? option : 0]; break;
    case Base: text = MeterStyles::paletteName(option); break;
    case Element: text = MeterAppearanceCatalog::role(MeterRole(option < MeterAppearance::Roles ? option : 0)).name; break;
    case Color: text = option ? MeterAppearanceCatalog::swatch(option - 1).name : "Default"; break;
    case Font: text = option && option <= info().fonts ? info().fontNames[option - 1] : "Default"; break;
    case Style: text = option && option <= info().styles ? info().styleNames[option - 1] : "Default"; break;
    default: break;
    }
    std::snprintf(output, capacity, "%s", text);
}
bool MeterAppearanceProperties::choose(unsigned index, unsigned option) {
    if (option >= options(index)) return false;
    auto &style = appearance_[element_];
    switch (at(index)) {
    case Preset:
        if (option == 1) {
            appearance_ = MeterAppearanceCatalog::hybrid();
            palette_ = MeterAppearanceCatalog::HybridPalette; face_ = MeterAppearanceCatalog::HybridFace;
        }
        return true; // Custom: what is set stays.
    case Base: palette_ = option; return true;
    case Element: element_ = MeterRole(option); return true;
    case Color: style.color = stored(option); return true;
    case Font: style.font = stored(option); return true;
    case Style: style.style = stored(option); return true;
    default: return false;
    }
}
void MeterAppearanceProperties::reset(Row row) {
    if (row == ResetAll) appearance_ = MeterAppearance();
    else if (row == ResetElement) appearance_[element_] = MeterRoleStyle();
}
} // namespace openosc::ui
