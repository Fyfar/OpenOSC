#include "openosc/ui/editors/arrangement_properties.hpp"
#include <cstdio>

namespace openosc::ui {
void ArrangementRows::name(unsigned slot, char *output, size_t capacity) {
    std::snprintf(output, capacity, "Position %u", slot + 1);
}
void ArrangementRows::blockName(const char *name, char *output, size_t capacity) {
    std::snprintf(output, capacity, "%s", name);
}
} // namespace openosc::ui
