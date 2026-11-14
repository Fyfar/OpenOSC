#pragma once
#include "openosc/settings/theme.hpp"

namespace openosc {
// Firmware before 77c6cd0 saved every element's geometry, so a stock Cards or Compact theme from
// it names the Scope plot and footer explicitly and would keep an older stock layout forever.
// A Scope waveform and footer that are exactly one of the earlier stock pairs take the current
// defaults of the theme's preset; palette, visibility, fonts and any custom geometry stay as saved.
// That includes the one-row footer with the plot before its marker margins (300x161 / 300x177).
class ThemeMigration {
public:
    static bool adoptStockScope(Theme &theme); // True when the Scope geometry was replaced.
    // A DMM reading and unit that are exactly an earlier stock pair (Cards/Compact 56 px or 40 px,
    // Legacy 32 px) take the current DMM body: their box and font, and those of the details line
    // and the summary when these are stock too. Colours, visibility and custom elements stay.
    static bool adoptStockMeter(Theme &theme);
    // A legacy-theme element at the stock Legacy geometry, current or earlier (the Scope waveform
    // was 10,38 300x152 before its marker margins; the DMM body was Legacy's own before it took
    // the Cards body).
    static bool legacyStock(unsigned mode, unsigned element, const ElementStyle &style, const ElementStyle &current);
};
} // namespace openosc
