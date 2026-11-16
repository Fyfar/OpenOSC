#pragma once
#include "openosc/settings/theme.hpp"
#include "openosc/settings/json_workspace.hpp"
#include <ArduinoJson.h>

namespace openosc {
// ArduinoJson slots differ between the MCU and native 64-bit tests.
inline constexpr size_t ThemeDocumentCapacity = JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(9)
    + InstrumentModeCount * (JSON_ARRAY_SIZE(ThemeInputElementLimit)
        + ThemeInputElementLimit * JSON_OBJECT_SIZE(10)) + 2048
    + JSON_OBJECT_SIZE(22)+JSON_ARRAY_SIZE(2)+2*JSON_OBJECT_SIZE(6)+JSON_OBJECT_SIZE(7)+512;
using ThemeDocument = StaticJsonDocument<ThemeDocumentCapacity>;

// Theme file operations run sequentially in the foreground. Their large scratch
// buffers must not share the DTCM stack with LVGL and the application controller: they are the
// JSON workspace's (JsonWorkspace), as the settings' and the calibration profiles'.
class ThemeScratch {
public:
    static ThemeDocument &document() { return JsonWorkspace::document<ThemeDocument>(); }
    static char *input() { return JsonWorkspace::text(); }
    static constexpr size_t InputCapacity = ThemeParser::MaxBytes + 1;
};
static_assert(ThemeScratch::InputCapacity <= JsonWorkspace::TextBytes, "The theme text exceeds the JSON workspace");
}
