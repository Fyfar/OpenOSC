#pragma once
#include <cstddef>
#include <cstdint>
#include "openosc/settings/color_value.hpp"

namespace openosc::ui {
// The colour rows every appearance list shares (DMM and Scope): Source (Default, the 16 named
// colours, Custom RGB) and, for Custom, R, G and B as Numbers 0..255 (Left/Right 1, Up/Down
// CoarseStep). The owning PropertyList forwards its rows here; MENU commits each choice or number
// edit, F4 restores it (PropertyNavigator); there is no Apply. `inherited` is the colour Default
// stands for, or Unknown (any value above 0xffffff: no sample, Custom starts from `start`).
class ColorRows {
public:
    enum Row : uint8_t { Source, Red, Green, Blue, Rows };
    static constexpr unsigned SourceOptions = 1 + ColorCatalog::Swatches + 1; // Default, named, Custom RGB.
    static constexpr unsigned CustomOption = SourceOptions - 1;
    static constexpr int CoarseStep = 16;
    static constexpr uint32_t Unknown = 0xffffffffu;
    static bool known(uint32_t rgb) { return rgb <= 0xffffff; }
    static unsigned count(const ColorValue &value) { return value.isCustom() ? Rows : 1; }
    static const char *name(Row row); // "Color", "Red", "Green", "Blue"
    static unsigned option(const ColorValue &value);
    // Option names: `inheritName` for Default ("Default", "Source", "Auto"), the colours, "Custom RGB".
    static const char *optionName(unsigned option, const char *inheritName = "Default");
    static bool optionSwatch(unsigned option, const ColorValue &value, uint32_t inherited, uint32_t &rgb);
    static bool valueSwatch(const ColorValue &value, uint32_t inherited, uint32_t &rgb);
    // Custom RGB from Default or a named colour starts from the colour shown (`inherited`, or
    // `start` when it is Unknown); Custom again keeps its channels.
    static bool choose(unsigned option, ColorValue &value, uint32_t inherited, uint32_t start = 0x808080);
    static int32_t number(Row row, const ColorValue &value);
    static void setNumber(Row row, int32_t number, ColorValue &value); // Clamped 0..255.
    static void adjust(Row row, int delta, ColorValue &value);           // Clamped, no wrap.
    static void text(Row row, const ColorValue &value, char *output, size_t capacity); // A channel's number.
};
} // namespace openosc::ui
