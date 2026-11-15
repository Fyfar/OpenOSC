#include "openosc/ui/editors/color_rows.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
uint8_t *channel(ColorRows::Row row, ColorValue &v) { return row == ColorRows::Red ? &v.r : row == ColorRows::Green ? &v.g : row == ColorRows::Blue ? &v.b : nullptr; }
} // namespace
const char *ColorRows::name(Row row) {
    static constexpr const char *names[Rows] = {"Color", "Red", "Green", "Blue"};
    return row < Rows ? names[row] : "";
}
unsigned ColorRows::option(const ColorValue &value) {
    if (value.isCustom()) return CustomOption;
    return value.index < ColorCatalog::Swatches ? value.index + 1U : 0;
}
const char *ColorRows::optionName(unsigned option, const char *inheritName) {
    if (!option || option > CustomOption) return inheritName;
    return option == CustomOption ? "Custom RGB" : ColorCatalog::swatch(option - 1).name;
}
bool ColorRows::optionSwatch(unsigned option, const ColorValue &value, uint32_t inherited, uint32_t &rgb) {
    if (!option) { rgb = inherited; return known(inherited); }
    if (option < CustomOption) { rgb = ColorCatalog::swatch(option - 1).rgb; return true; }
    if (option == CustomOption) { // Its channels, or what choosing it would start from.
        rgb = value.isCustom() ? value.rgb() : value.resolve(inherited);
        return known(rgb);
    }
    return false;
}
bool ColorRows::valueSwatch(const ColorValue &value, uint32_t inherited, uint32_t &rgb) {
    rgb = value.resolve(inherited);
    return known(rgb);
}
bool ColorRows::choose(unsigned option, ColorValue &value, uint32_t inherited, uint32_t start) {
    if (option >= SourceOptions) return false;
    if (!option) value = ColorValue();
    else if (option < CustomOption) value = ColorValue::named(uint8_t(option - 1));
    else if (!value.isCustom()) {
        const uint32_t shown = value.resolve(inherited);
        value = ColorValue::custom(known(shown) ? shown : start);
    }
    return true;
}
int32_t ColorRows::number(Row row, const ColorValue &value) {
    auto copy = value;
    const auto *c = channel(row, copy);
    return c ? *c : 0;
}
void ColorRows::setNumber(Row row, int32_t number, ColorValue &value) {
    if (auto *c = channel(row, value)) *c = uint8_t(number < 0 ? 0 : number > 255 ? 255 : number);
}
void ColorRows::adjust(Row row, int delta, ColorValue &value) { setNumber(row, number(row, value) + delta, value); }
void ColorRows::text(Row row, const ColorValue &value, char *output, size_t capacity) {
    std::snprintf(output, capacity, "%d", int(number(row, value)));
}
} // namespace openosc::ui
