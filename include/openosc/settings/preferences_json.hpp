#pragma once
#include "openosc/settings/preferences.hpp"
#include <cstddef>

namespace openosc {
class PreferencesJson {
public:
    // The whole file: about 1.8 KB pretty; with every DMM and Scope colour custom, every font and
    // style set and non-default block orders, 4.1 KB pretty or 2.4 KB compact. A file that would not
    // fit pretty is written compact (no field is ever dropped); older firmware still reads it.
    static constexpr size_t MaxBytes = 4096;
    // The JSON tree of the largest file: 155 members/elements and 1.7 KB of copied strings (keys
    // and values), with headroom. In ArduinoJson slots, so a 32-bit MCU needs 224 * 16 + 2560 =
    // 6144 bytes (as before) and a 64-bit host proportionally more.
    static constexpr size_t DocumentSlots = 224, StringBytes = 2560;
    static bool parse(const char* input, size_t bytes, Preferences& output);
    static size_t encode(const Preferences& values, char* output, size_t capacity,bool pretty=true);
};
}
