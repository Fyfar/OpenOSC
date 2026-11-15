#include "openosc/settings/meter_appearance.hpp"
#include <cstring>

namespace openosc {
namespace {
constexpr const char *readingFonts[] = {"IBM", "Mono", "Segment"}, *readingKeys[] = {"ibm", "mono", "segment"};
constexpr const char *textFonts[] = {"IBM", "Mono"}, *textKeys[] = {"ibm", "mono"};
constexpr const char *lines[] = {"Thin", "Bold"}, *lineKeys[] = {"thin", "bold"};
constexpr MeterRoleInfo color(const char *name, const char *key) { return {name, key, 0, nullptr, nullptr, 0, nullptr, nullptr}; }
constexpr MeterRoleInfo line(const char *name, const char *key) { return {name, key, 0, nullptr, nullptr, 2, lines, lineKeys}; }
constexpr MeterRoleInfo text(const char *name, const char *key) { return {name, key, 2, textFonts, textKeys, 0, nullptr, nullptr}; }
constexpr MeterRoleInfo roles[MeterAppearance::Roles] = {
    color("Background", "background"),
    {"Reading", "reading", 3, readingFonts, readingKeys, 0, nullptr, nullptr},
    text("Unit", "unit"),
    color("Function sign", "function"),
    line("Dial arc", "arc"), line("Dial ticks", "ticks"), text("Scale numbers", "scale_numbers"), line("Needle", "needle"),
    text("Stat names", "stat_names"), text("MIN value", "minimum"), text("MAX value", "maximum"), text("AVG value", "average"),
    text("Range / REL", "range"), text("Secondary", "secondary"), text("AUTO / MANUAL", "mode"), text("Info line", "details"),
    color("Bar / trend", "bar"),
    color("Card sign lit", "card_active"), color("Card signs", "card_inactive"), text("Card text", "card_text"),
    color("Card frame", "card_selection")};
} // namespace
uint8_t MeterAppearanceCatalog::find(const char *const *keys, unsigned count, const char *key) {
    for (unsigned i = 0; key && i < count; ++i) if (!std::strcmp(keys[i], key)) return uint8_t(i);
    return MeterRoleStyle::Inherit;
}
uint8_t MeterAppearanceCatalog::swatchByKey(const char *key) { return ColorCatalog::byKey(key); }
const MeterSwatch &MeterAppearanceCatalog::swatch(unsigned index) { return ColorCatalog::swatch(index); }
const MeterRoleInfo &MeterAppearanceCatalog::role(MeterRole role) { return roles[unsigned(role) < MeterAppearance::Roles ? unsigned(role) : 0]; }
bool MeterAppearance::custom() const {
    for (const auto &role : roles) if (!(role == MeterRoleStyle{})) return true;
    return false;
}
bool MeterAppearance::valid() const {
    for (unsigned i = 0; i < Roles; ++i) {
        const auto &r = roles[i];
        const auto &info = MeterAppearanceCatalog::role(MeterRole(i));
        if (!r.colorValue().valid() ||
            (r.font != MeterRoleStyle::Inherit && r.font >= info.fonts) || (r.style != MeterRoleStyle::Inherit && r.style >= info.styles))
            return false;
    }
    return true;
}
bool MeterAppearance::operator==(const MeterAppearance &b) const {
    for (unsigned i = 0; i < Roles; ++i) if (!(roles[i] == b.roles[i])) return false;
    return true;
}
MeterAppearance MeterAppearanceCatalog::hybrid() {
    MeterAppearance a;
    const auto set = [&](MeterRole role, const char *swatch) { a[role].color = swatchByKey(swatch); };
    set(MeterRole::Background, "black");
    set(MeterRole::Reading, "amber"); set(MeterRole::Unit, "amber");
    a[MeterRole::Reading].font = ReadingIbm; a[MeterRole::Unit].font = TextIbm; // IBM, whatever the face.
    set(MeterRole::Function, "white");
    set(MeterRole::Arc, "white"); set(MeterRole::Ticks, "white"); set(MeterRole::ScaleNumbers, "white");
    set(MeterRole::Needle, "orange"); a[MeterRole::Needle].style = Bold;
    set(MeterRole::StatNames, "gray");
    set(MeterRole::Minimum, "silver"); set(MeterRole::Maximum, "silver"); set(MeterRole::Average, "silver");
    set(MeterRole::Range, "gray"); set(MeterRole::Secondary, "silver"); set(MeterRole::Mode, "sky"); set(MeterRole::Details, "gray");
    set(MeterRole::Bar, "amber");
    set(MeterRole::CardActive, "cyan"); set(MeterRole::CardInactive, "gray"); set(MeterRole::CardText, "silver");
    set(MeterRole::CardSelection, "cyan");
    return a;
}
} // namespace openosc
