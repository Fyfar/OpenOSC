#pragma once
#include "openosc/settings/preferences.hpp"
#include <ArduinoJson.h>

namespace openosc {
class InstrumentJson {
public:
    static bool parse(JsonObjectConst scope, JsonObjectConst meter, Preferences &output);
    static void encode(JsonObject scope, JsonObject meter, const Preferences &values);
};
} // namespace openosc
