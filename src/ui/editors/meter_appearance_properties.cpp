#include "openosc/ui/editors/meter_appearance_properties.hpp"
#include "openosc/ui/components/meter_style.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
constexpr const char *presets[] = {"Custom", "Hybrid"};
// Choice option 0 is the inherited value; 1.. the role's swatches, fonts or styles.
unsigned choice(uint8_t value) { return value == MeterRoleStyle::Inherit ? 0 : value + 1U; }
uint8_t stored(unsigned option) { return option ? uint8_t(option - 1) : MeterRoleStyle::Inherit; }
using Props = MeterAppearanceProperties;
bool channelRow(Props::Row row) { return row >= Props::Red && row <= Props::Blue; }
ColorRows::Row colorRow(Props::Row row) { return ColorRows::Row(row - Props::Color); }
} // namespace
bool MeterAppearanceProperties::shown(Row row) const {
    if (channelRow(row)) return color().isCustom();
    return !((row == Font && !info().fonts) || (row == Style && !info().styles));
}
MeterAppearanceProperties::Row MeterAppearanceProperties::at(unsigned index) const {
    unsigned visible = 0;
    for (unsigned r = 0; r < Rows; ++r) {
        if (!shown(Row(r))) continue;
        if (visible++ == index) return Row(r);
    }
    return Rows;
}
unsigned MeterAppearanceProperties::index(Row row) const {
    for (unsigned i = 0; i < count(); ++i) if (at(i) == row) return i;
    return count();
}
unsigned MeterAppearanceProperties::count() const {
    unsigned n = 0;
    for (unsigned r = 0; r < Rows; ++r) n += shown(Row(r));
    return n;
}
bool MeterAppearanceProperties::hybrid() const {
    return palette_ == MeterAppearanceCatalog::HybridPalette && face_ == MeterAppearanceCatalog::HybridFace &&
           appearance_ == MeterAppearanceCatalog::hybrid();
}
PropertyKind MeterAppearanceProperties::kind(unsigned index) const {
    const auto row = at(index);
    if (row == ResetElement || row == ResetAll) return PropertyKind::Action;
    return channelRow(row) ? PropertyKind::Number : PropertyKind::Choice;
}
void MeterAppearanceProperties::name(unsigned index, char *output, size_t capacity) const {
    static constexpr const char *names[Rows] = {"Preset", "Base colors", "Element", "Color", "Red", "Green", "Blue", "Font", "Style", "Reset element", "Reset all"};
    const auto row = at(index);
    std::snprintf(output, capacity, "%s", row < Rows ? names[row] : "");
}
unsigned MeterAppearanceProperties::options(unsigned index) const {
    switch (at(index)) {
    case Preset: return 2;
    case Base: return MeterStyles::Schemes;
    case Element: return MeterAppearance::Roles;
    case Color: return ColorRows::SourceOptions;
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
    case Color: return ColorRows::option(style.colorValue());
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
    case Color: text = ColorRows::optionName(option); break;
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
    case Color: {
        auto value = style.colorValue();
        if (!ColorRows::choose(option, value, inherited(element_))) return false;
        style.setColor(value);
        return true;
    }
    case Font: style.font = stored(option); return true;
    case Style: style.style = stored(option); return true;
    default: return false;
    }
}
uint32_t MeterAppearanceProperties::inherited(MeterRole role) const {
    if (!theme_) return ColorRows::Unknown;
    return MeterStyles::look(palette_, face_, MeterAppearance(), *theme_, parameters_).color(role);
}
void MeterAppearanceProperties::value(unsigned index, char *output, size_t capacity) const {
    const auto row = at(index);
    if (channelRow(row)) ColorRows::text(colorRow(row), color(), output, capacity);
    else PropertyList::value(index, output, capacity);
}
void MeterAppearanceProperties::adjust(unsigned index, int delta) {
    const auto row = at(index);
    if (!channelRow(row)) return;
    auto value = color();
    ColorRows::adjust(colorRow(row), delta, value);
    appearance_[element_].setColor(value);
}
int MeterAppearanceProperties::coarseStep(unsigned index) const { return channelRow(at(index)) ? ColorRows::CoarseStep : 1; }
int32_t MeterAppearanceProperties::number(unsigned index) const {
    const auto row = at(index);
    return channelRow(row) ? ColorRows::number(colorRow(row), color()) : 0;
}
void MeterAppearanceProperties::setNumber(unsigned index, int32_t number) {
    const auto row = at(index);
    if (!channelRow(row)) return;
    auto value = color();
    ColorRows::setNumber(colorRow(row), number, value);
    appearance_[element_].setColor(value);
}
bool MeterAppearanceProperties::optionSwatch(unsigned index, unsigned option, uint32_t &rgb) const {
    switch (at(index)) {
    case Base: // The palette's body.
        if (!theme_ || option >= MeterStyles::Schemes) return false;
        rgb = MeterStyles::palette(option, *theme_).background;
        return true;
    case Element: { // Each element as it is drawn now.
        if (option >= MeterAppearance::Roles) return false;
        const auto role = MeterRole(option);
        return ColorRows::valueSwatch(appearance_[role].colorValue(), inherited(role), rgb);
    }
    case Color: return ColorRows::optionSwatch(option, color(), inherited(element_), rgb);
    default: return false;
    }
}
bool MeterAppearanceProperties::valueSwatch(unsigned index, uint32_t &rgb) const {
    const auto row = at(index);
    if (row == Base || row == Element) return optionSwatch(index, option(index), rgb);
    if (row == Color || channelRow(row)) return ColorRows::valueSwatch(color(), inherited(element_), rgb);
    return false;
}
void MeterAppearanceProperties::reset(Row row) {
    if (row == ResetAll) appearance_ = MeterAppearance();
    else if (row == ResetElement) appearance_[element_] = MeterRoleStyle();
}
} // namespace openosc::ui
