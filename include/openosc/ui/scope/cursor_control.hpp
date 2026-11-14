#pragma once
#include "openosc/ui/scope/options.hpp"

namespace openosc::plot {
// Which cursor line the arrows move; the lines themselves stay in CursorOptions.
class CursorControl {
public:
    static bool available(const Options &options);
    static void cycleMode(CursorOptions &cursors, int direction); // Off, Time, Level, both.
    static const char *lineName(CursorLine line, bool spectrum);
    void enter(const CursorOptions &cursors);
    void next(const CursorOptions &cursors);
    void leave() { line_ = CursorLine::None; }
    void sync(const CursorOptions &cursors); // A hidden line gives way to the first one still drawn.
    CursorLine line() const { return line_; }
    // One pixel at a time at any plot size, so that a step never leaves the line where it was.
    bool move(CursorOptions &cursors, int horizontal, int vertical, bool fine, unsigned width, unsigned height) const;

private:
    CursorLine line_ = CursorLine::None;
};
} // namespace openosc::plot
