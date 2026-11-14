#pragma once
#include "openosc/ui/scope/viewport.hpp"

namespace openosc::plot {
// The plot columns of a time window that lie outside its record (panned or zoomed past an end):
// columns [0, before) come before the record's first point, [width - after, width) after its
// last. These are exactly the columns projectTime() finds no record point for: an empty
// Viewport::column when aggregating, a position outside Source::interpolate's bounds otherwise.
// A window entirely outside the record has all its columns on one side.
struct RecordGaps {
    unsigned width = 0, before = 0, after = 0;
    static RecordGaps of(const Viewport &viewport, uint32_t samples);
    bool any() const { return before || after; }
    bool whole() const { return width && before + after >= width; }
};
} // namespace openosc::plot
