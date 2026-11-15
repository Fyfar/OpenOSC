#include "openosc/ui/editors/scope_appearance_properties.hpp"
#include "openosc/ui/components/scope_styles.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
constexpr unsigned Schemes = unsigned(ScopeScheme::Count);
ColorRows::Row colorRow(ScopeAppearanceProperties::Row row) { return ColorRows::Row(row - ScopeAppearanceProperties::Source); }
bool channelRow(ScopeAppearanceProperties::Row row) { return row >= ScopeAppearanceProperties::Red && row <= ScopeAppearanceProperties::Blue; }
} // namespace
ScopeAppearanceProperties::Row ScopeAppearanceProperties::at(unsigned index) const {
    const bool custom = appearance_[element_].isCustom();
    unsigned shown = 0;
    for (unsigned r = 0; r < Rows; ++r) {
        if (channelRow(Row(r)) && !custom) continue;
        if (shown++ == index) return Row(r);
    }
    return Rows;
}
unsigned ScopeAppearanceProperties::index(Row row) const {
    for (unsigned i = 0; i < count(); ++i) if (at(i) == row) return i;
    return count();
}
unsigned ScopeAppearanceProperties::count() const { return Rows - (appearance_[element_].isCustom() ? 0 : 3); }
void ScopeAppearanceProperties::reset(Row row) {
    if (row == ResetAll) { const auto scheme = appearance_.scheme; appearance_ = ScopeAppearance(); appearance_.scheme = scheme; }
    else if (row == ResetElement) appearance_[element_] = ColorValue();
}
uint32_t ScopeAppearanceProperties::inherited(ScopeRole role) const {
    // Readout inherits the resolved Text (its override included); the others the scheme's colour.
    const uint32_t rgb = role == ScopeRole::Readout ? appearance_[ScopeRole::Text].resolve(ScopeStyles::base(appearance_.scheme, ScopeRole::Text, *theme_))
                                                    : ScopeStyles::base(appearance_.scheme, role, *theme_);
    return ColorRows::known(rgb) ? rgb : ColorRows::Unknown;
}
uint32_t ScopeAppearanceProperties::start() const {
    if (element_ == ScopeRole::TagText) return ColorContrast::ink(inherited(ScopeRole::Background) & 0xffffff);
    return appearance_[ScopeRole::Channel1].resolve(inherited(ScopeRole::Channel1)); // Trigger: CH1's colour.
}
PropertyKind ScopeAppearanceProperties::kind(unsigned index) const {
    const auto row = at(index);
    if (row == ResetElement || row == ResetAll) return PropertyKind::Action;
    return channelRow(row) ? PropertyKind::Number : PropertyKind::Choice;
}
void ScopeAppearanceProperties::name(unsigned index, char *output, size_t capacity) const {
    static constexpr const char *names[] = {"Scheme", "Element", "", "", "", "", "Reset element", "Reset all"};
    const auto row = at(index);
    const char *text = row >= Source && row <= Blue ? ColorRows::name(colorRow(row)) : row < Rows ? names[row] : "";
    std::snprintf(output, capacity, "%s", text);
}
void ScopeAppearanceProperties::value(unsigned index, char *output, size_t capacity) const {
    const auto row = at(index);
    if (channelRow(row)) ColorRows::text(colorRow(row), appearance_[element_], output, capacity);
    else PropertyList::value(index, output, capacity);
}
unsigned ScopeAppearanceProperties::options(unsigned index) const {
    switch (at(index)) {
    case Scheme: return Schemes;
    case Element: return ScopeAppearance::Roles;
    case Source: return ColorRows::SourceOptions;
    default: return 0;
    }
}
unsigned ScopeAppearanceProperties::option(unsigned index) const {
    switch (at(index)) {
    case Scheme: return unsigned(appearance_.scheme);
    case Element: return unsigned(element_);
    case Source: return ColorRows::option(appearance_[element_]);
    default: return 0;
    }
}
void ScopeAppearanceProperties::optionName(unsigned index, unsigned option, char *output, size_t capacity) const {
    const char *text = "";
    switch (at(index)) {
    case Scheme: text = ScopeAppearanceCatalog::schemeName(ScopeScheme(option < Schemes ? option : 0)); break;
    case Element: text = ScopeAppearanceCatalog::role(ScopeRole(option < ScopeAppearance::Roles ? option : 0)).name; break;
    case Source:
        text = ColorRows::optionName(option, element_ == ScopeRole::Trigger ? "Source" : element_ == ScopeRole::TagText ? "Auto" : "Default");
        break;
    default: break;
    }
    std::snprintf(output, capacity, "%s", text);
}
bool ScopeAppearanceProperties::choose(unsigned index, unsigned option) {
    if (option >= options(index)) return false;
    switch (at(index)) {
    case Scheme: appearance_.scheme = ScopeScheme(option); return true;
    case Element: element_ = ScopeRole(option); return true;
    case Source: return ColorRows::choose(option, appearance_[element_], inherited(element_), start());
    default: return false;
    }
}
void ScopeAppearanceProperties::adjust(unsigned index, int delta) {
    const auto row = at(index);
    if (channelRow(row)) ColorRows::adjust(colorRow(row), delta, appearance_[element_]);
}
int ScopeAppearanceProperties::coarseStep(unsigned index) const { return channelRow(at(index)) ? ColorRows::CoarseStep : 1; }
int32_t ScopeAppearanceProperties::number(unsigned index) const {
    const auto row = at(index);
    return channelRow(row) ? ColorRows::number(colorRow(row), appearance_[element_]) : 0;
}
void ScopeAppearanceProperties::setNumber(unsigned index, int32_t value) {
    const auto row = at(index);
    if (channelRow(row)) ColorRows::setNumber(colorRow(row), value, appearance_[element_]);
}
bool ScopeAppearanceProperties::optionSwatch(unsigned index, unsigned option, uint32_t &rgb) const {
    switch (at(index)) {
    case Scheme: // The scheme's body.
        if (option >= Schemes) return false;
        rgb = ScopeStyles::base(ScopeScheme(option), ScopeRole::Background, *theme_);
        return true;
    case Element: { // Each element as it is drawn now.
        if (option >= ScopeAppearance::Roles) return false;
        const auto role = ScopeRole(option);
        return ColorRows::valueSwatch(appearance_[role], inherited(role), rgb);
    }
    case Source: return ColorRows::optionSwatch(option, appearance_[element_], inherited(element_), rgb);
    default: return false;
    }
}
bool ScopeAppearanceProperties::valueSwatch(unsigned index, uint32_t &rgb) const {
    const auto row = at(index);
    if (row == Scheme || row == Element) return optionSwatch(index, option(index), rgb);
    if (row >= Source && row <= Blue) return ColorRows::valueSwatch(appearance_[element_], inherited(element_), rgb);
    return false;
}
} // namespace openosc::ui
