#pragma once

namespace openosc::ui {
struct StatusColumn {
    int x, width;
    constexpr int right() const { return x + width; } // First column past it.
};
// Columns of the persistent 20-pixel status bar. From the left: the state pill, then the trigger
// edge and level and the FPS readout. From the right: battery at the panel's edge, the record
// overview, then the GEN and USB icons. Reserve the widest battery mode so changing between
// icon, volts and percent never moves the other indicators. No pill reaches this fixed right
// side. The trigger takes
// the first of two fixed places the pill leaves free, FPS shows where nothing else is drawn.
// Widths are IBM Plex Sans 12 extents of the widest texts: "4.12V", "2 -88.8mV", "125 fps"
// (LVGL may refresh every 8 ms), "48.8K"; tests/native/scope/status_bar.cpp renders them all.
struct StatusBarLayout {
    using Column = StatusColumn;
    static constexpr int Width = 320, Height = 20, Margin = 2, Gap = 3, Tight = 2;
    static constexpr int PillPadding = 4; // Each side of the state text.
    static constexpr Column BatteryIcon{Width - Margin - 15, 15}, BatteryText{Width - Margin - 32, 32};
    static constexpr int StatusX = Margin;
    // Record overview: the total points of the frame on show, then the map of that record.
    static constexpr Column Overview{BatteryText.x - Gap - 96, 96};
    static constexpr Column OverviewCaption{Overview.x, 32};
    static constexpr Column OverviewMap{OverviewCaption.right() + Tight, Overview.right() - OverviewCaption.right() - Tight};
    static constexpr int IconGap = 4;
    static constexpr Column Generator{Overview.x - IconGap - 9, 9};
    static constexpr Column Usb{Generator.x - IconGap - 9, 9};
    static constexpr Column Fps{Usb.x - Gap - 41, 41}; // Right-aligned text.
    // Trigger edge glyph and level: near the pill beside the short states (RUN, WAIT, HOLD, ROLL,
    // and SINGLE), one fixed step further beside the medium ones.
    static constexpr int EdgeWidth = 8, LevelWidth = 58, TriggerWidth = EdgeWidth + Tight + LevelWidth;
    static constexpr Column TriggerNear{45, TriggerWidth}, TriggerFar{75, TriggerWidth};
    static constexpr Column edge(Column trigger) { return {trigger.x, EdgeWidth}; }
    static constexpr Column level(Column trigger) { return {trigger.x + EdgeWidth + Tight, LevelWidth}; }
    // The pill is clipped here rather than run into the USB icon.
    static constexpr int MaximumStatus = Usb.x - Gap - StatusX;

    static constexpr Column battery(bool text) { return text ? BatteryText : BatteryIcon; }
    // Whether a pill ending at statusRight leaves the column visible.
    static constexpr bool clear(int statusRight, Column column) { return statusRight + Gap <= column.x; }
    // Where the trigger goes beside a pill ending at statusRight; false when neither place is free.
    static constexpr bool trigger(int statusRight, Column &out) {
        if (clear(statusRight, TriggerNear)) { out = TriggerNear; return true; }
        if (clear(statusRight, TriggerFar)) { out = TriggerFar; return true; }
        return false;
    }
};
namespace status_bar_checks {
using L = StatusBarLayout;
static_assert(L::BatteryIcon.right() + L::Margin == L::Width && L::BatteryText.right() == L::BatteryIcon.right(), "Battery at the right edge in every mode");
static_assert(L::OverviewMap.width >= 60 && L::Overview.right() + L::Gap == L::BatteryText.x, "Overview clears every battery mode");
static_assert(L::Generator.right() + L::IconGap == L::Overview.x, "GEN reaches the overview");
static_assert(L::Usb.right() + L::IconGap == L::Generator.x, "USB reaches GEN");
static_assert(L::TriggerNear.right() + L::Tight <= L::Fps.x, "The near trigger reaches FPS");
static_assert(L::TriggerFar.right() + L::Gap <= L::Usb.x, "The far trigger reaches USB");
static_assert(L::StatusX + 40 + L::Gap <= L::TriggerNear.x, "\"HOLD\" keeps the near trigger");
static_assert(L::StatusX + 70 + L::Gap <= L::TriggerFar.x, "\"HOLD RAW\" keeps the far trigger");
static_assert(L::MaximumStatus >= 154, "\"CAPTURE CONFIG ERROR\" must stay readable");
} // namespace status_bar_checks
} // namespace openosc::ui
