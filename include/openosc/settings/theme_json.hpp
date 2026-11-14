#pragma once
#include "openosc/settings/theme.hpp"

namespace openosc {
class ThemeJson {
public:
    static size_t encode(const Theme& theme, char* output, size_t capacity);
};
}
