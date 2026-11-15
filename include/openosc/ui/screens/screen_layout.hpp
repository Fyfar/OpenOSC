#pragma once
#include "openosc/settings/screen_arrangement.hpp"
#include "openosc/ui/components/scope_footer_layout.hpp"
#include "openosc/ui/components/status_bar_layout.hpp"
#include "openosc/ui/scope/plot_frame.hpp"

namespace openosc::ui {
// Where each block of a screen stands: the blocks of an order (screen_arrangement.hpp) stacked top
// to bottom under the status bar, every block at its own fixed height (nothing is scaled to make
// room). The rows left over become small equal gaps between the blocks, so the first block starts
// right under the status bar and the last one ends at the bottom edge; an absent block (hidden:
// Statistics Off) takes no rows and leaves no hole. Pure geometry, no LVGL objects.
struct BlockSpan {
    int y = 0, height = 0;
    bool shown = false;
    constexpr int bottom() const { return y + height - 1; }
};
class ScreenLayout {
public:
    static constexpr int Top = StatusBarLayout::Height, Bottom = 240, Body = Bottom - Top; // y 20..239.
    // The soft keys: the same bar wherever Buttons stands, four equal 76 px zones 4 px apart.
    static constexpr int ButtonsX = 2, ButtonsWidth = 316, ButtonsHeight = 32, ButtonGap = 4;
    static constexpr int ButtonWidth = (ButtonsWidth - 3 * ButtonGap) / 4;
    // Scope: the readouts row (ScopeFooter, as high as the status bar) and the waveform object:
    // its marker margin above the 156-row plot (PlotFrame::Top), nothing below it.
    static constexpr int Readouts = ScopeFooterLayout::Height, PlotHeight = 156, Waveform = plot::PlotFrame::Top + PlotHeight;
    // The DMM's blocks. Digital: the reading block is the function sign row (sign, jacks, unit
    // badge) over the 64 px digits; the measurements block is the meta row, MIN/MAX/AVG and the bar
    // (without the statistics only the meta row and the bar). Analog: the dial block is the sign
    // and meta row over the 92 px dial; measurements is MIN/MAX/AVG on one row; reading is the 64 px
    // digits with their unit.
    static constexpr int SignRow = 27, Digits = 64, DigitalReading = SignRow + Digits;
    static constexpr int DigitalMeasurements = 89, DigitalMeterNoStatistics = 50;
    static constexpr int MetaRow = 14, DialOffset = 9, DialHeight = 92, AnalogDial = DialOffset + DialHeight;
    static constexpr int AnalogMeasurements = 14, AnalogReading = 62;

    template<class Block>
    static void stack(const BlockOrder<Block> &order, const int (&heights)[BlockOrder<Block>::Size],
                      BlockSpan (&spans)[BlockOrder<Block>::Size]) {
        constexpr unsigned N = BlockOrder<Block>::Size;
        unsigned shown = 0;
        int used = 0;
        for (unsigned i = 0; i < N; ++i) {
            spans[i] = {};
            if (heights[i] > 0) { ++shown; used += heights[i]; }
        }
        const int free = Body - used, gaps = shown > 1 ? int(shown) - 1 : 1;
        int y = Top, gap = 0;
        for (unsigned slot = 0; slot < N; ++slot) {
            const unsigned block = unsigned(order.at(slot));
            if (heights[block] <= 0) continue;
            y += gap > 0 ? free / gaps + (gap <= free % gaps ? 1 : 0) : 0;
            spans[block] = {y, heights[block], true};
            y += heights[block];
            ++gap;
        }
    }
    // Scope: [Measurements, Waveform, Buttons] spans by ScopeBlock; `readouts` false: no readouts row
    // (it takes no rows).
    static void scope(const ScopeArrangement &order, bool readouts, BlockSpan (&spans)[ScopeArrangement::Size]) {
        const int heights[] = {readouts ? Readouts : 0, Waveform, ButtonsHeight};
        stack(order, heights, spans);
    }
    static void digital(const DigitalArrangement &order, bool statistics, BlockSpan (&spans)[DigitalArrangement::Size]) {
        const int heights[] = {DigitalReading, statistics ? DigitalMeasurements : DigitalMeterNoStatistics, ButtonsHeight};
        stack(order, heights, spans);
    }
    static void analog(const AnalogArrangement &order, bool statistics, BlockSpan (&spans)[AnalogArrangement::Size]) {
        const int heights[] = {AnalogDial, statistics ? AnalogMeasurements : 0, AnalogReading, ButtonsHeight};
        stack(order, heights, spans);
    }
};
static_assert(ScreenLayout::ButtonWidth == 76, "four equal 76 px zones");
static_assert(ScreenLayout::Readouts + ScreenLayout::Waveform + ScreenLayout::ButtonsHeight <= ScreenLayout::Body, "the Scope fits");
static_assert(ScreenLayout::DigitalReading + ScreenLayout::DigitalMeasurements + ScreenLayout::ButtonsHeight <= ScreenLayout::Body, "digital fits");
static_assert(ScreenLayout::AnalogDial + ScreenLayout::AnalogMeasurements + ScreenLayout::AnalogReading + ScreenLayout::ButtonsHeight <=
              ScreenLayout::Body, "analog fits");
} // namespace openosc::ui
