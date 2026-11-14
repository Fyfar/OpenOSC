#pragma once
#include "openosc/settings/preferences.hpp"
#include <cstddef>

namespace openosc {
class PreferencesJson {
public:
    // The whole file, pretty: about 1.6 KB, 3.2 KB with every DMM appearance override.
    static constexpr size_t MaxBytes = 4096;
    static bool parse(const char* input, size_t bytes, Preferences& output);
    static size_t encode(const Preferences& values, char* output, size_t capacity,bool pretty=true);
};
}
