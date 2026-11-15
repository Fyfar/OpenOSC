#include "openosc/settings/scope_appearance_json.hpp"

namespace openosc {
void ScopeAppearanceJson::parse(JsonObjectConst object, ScopeAppearance &output) {
    output = ScopeAppearance();
    if (object.isNull()) return;
    output.scheme = ScopeAppearanceCatalog::schemeByKey(object["scheme"].as<const char *>());
    const auto roles = object["roles"].as<JsonObjectConst>();
    if (roles.isNull()) return;
    for (const auto pair : roles) {
        const auto role = ScopeAppearanceCatalog::roleByKey(pair.key().c_str());
        if (role != ScopeRole::Count) output[role] = ColorText::parse(pair.value().as<const char *>());
    }
}
void ScopeAppearanceJson::encode(JsonObject parent, const ScopeAppearance &values) {
    if (values == ScopeAppearance()) return;
    auto object = parent.createNestedObject("appearance");
    if (values.scheme != ScopeScheme::Current) object["scheme"] = ScopeAppearanceCatalog::schemeKey(values.scheme);
    if (!values.custom()) return;
    auto roles = object.createNestedObject("roles");
    for (unsigned i = 0; i < ScopeAppearance::Roles; ++i) {
        char text[ColorText::Capacity];
        // Keys are static strings; the "#rrggbb" text is copied by ArduinoJson (a char array).
        if (ColorText::format(values.roles[i], text, sizeof text)) roles[ScopeAppearanceCatalog::role(ScopeRole(i)).key] = text;
    }
}
} // namespace openosc
