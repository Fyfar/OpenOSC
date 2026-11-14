#include "openosc/ui/editors/property_list.hpp"
#include <cstdio>

namespace openosc::ui {
void PropertyList::value(unsigned row, char *output, size_t capacity) const {
    const auto k = kind(row);
    if (k == PropertyKind::Choice || k == PropertyKind::Toggle) optionName(row, option(row), output, capacity);
    else if (capacity) output[0] = 0;
}
void PropertyList::optionName(unsigned row, unsigned index, char *output, size_t capacity) const {
    if (kind(row) == PropertyKind::Toggle) std::snprintf(output, capacity, "%s", index ? "On" : "Off");
    else if (capacity) output[0] = 0;
}
bool PropertyList::choose(unsigned row, unsigned index) {
    if (index >= options(row)) return false;
    for (unsigned guard = options(row); option(row) != index && guard; --guard) {
        const unsigned before = option(row);
        adjust(row, index > before ? 1 : -1);
        if (option(row) == before) return false; // Refused (a notice says why) or at its limit.
    }
    return option(row) == index;
}
} // namespace openosc::ui
