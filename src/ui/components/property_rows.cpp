#include "openosc/ui/components/property_rows.hpp"
#include <cstdio>

namespace openosc::ui {
void PropertyRows::draw(MenuContent &content, lv_obj_t *parent, const PropertyList &list, const PropertyNavigator &navigator,
                        const Theme &theme, uint32_t accent) const {
    const unsigned start = first(navigator.row());
    for (unsigned i = start; i < list.count() && i < start + visible; ++i) {
        char name[64], current[60], value[68];
        list.name(i, name, sizeof name);
        list.value(i, current, sizeof current);
        const auto kind = list.kind(i);
        const bool editing = i == navigator.row() && navigator.mode() == PropertyNavigator::Mode::Number;
        std::snprintf(value, sizeof value, editing ? "< %s >" : "%s", kind == PropertyKind::Page && !current[0] ? ">" : current);
        auto &row = content.row(parent, top + int(i - start) * pitch, height, name, value, i == navigator.row(), theme, accent);
        int32_t minimum = 0, maximum = 0;
        if (kind == PropertyKind::Toggle) row.toggle(list.option(i));
        else if (kind == PropertyKind::Number && !editing && list.range(i, minimum, maximum))
            row.slider(unsigned(list.number(i) - minimum), 0, unsigned(maximum - minimum));
    }
}
} // namespace openosc::ui
