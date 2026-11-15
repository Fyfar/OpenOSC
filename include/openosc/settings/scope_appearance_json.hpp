#pragma once
#include <ArduinoJson.h>
#include "openosc/settings/scope_appearance.hpp"

namespace openosc {
// scope.appearance: {"scheme": "daylight", "roles": {"<role key>": "<catalog key>|#rrggbb", ...}}.
// Only overrides are written; a missing object, unknown scheme, role or colour is the default, so
// files from before (and files edited by hand) always load. Where it lives in the file is the
// integrator's choice (InstrumentJson).
struct ScopeAppearanceJson {
    static void parse(JsonObjectConst object, ScopeAppearance &output);
    static void encode(JsonObject parent, const ScopeAppearance &values); // Writes parent["appearance"] if not default.
};
} // namespace openosc
