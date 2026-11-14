#include "openosc/ui/scope/cursor_control.hpp"
#include <algorithm>

namespace openosc::plot {
namespace {
bool shown(CursorLine line, const CursorOptions &c) {
    return line == CursorLine::TimeA || line == CursorLine::TimeB ? c.time : line != CursorLine::None && c.voltage;
}
void step(uint16_t &permille, int pixels, unsigned span) {
    if (!span || !pixels) return;
    const int pixel = cursorPixel(permille, int(span)) + pixels;
    permille = uint16_t(std::clamp<int>((pixel * 2000 + int(span)) / int(2 * span), 0, 1000));
}
} // namespace
bool CursorControl::available(const Options &options) {
    return options.mode != Mode::XY && (options.cursors.time || options.cursors.voltage);
}
void CursorControl::cycleMode(CursorOptions &c, int direction) {
    const int mode = (int(c.time) | int(c.voltage) << 1) + (direction < 0 ? -1 : 1);
    c.time = mode & 1; c.voltage = mode & 2;
}
const char *CursorControl::lineName(CursorLine line, bool spectrum) {
    switch (line) {
    case CursorLine::TimeA: return spectrum ? "Freq A" : "Time A";
    case CursorLine::TimeB: return spectrum ? "Freq B" : "Time B";
    case CursorLine::LevelA: return "Level a";
    case CursorLine::LevelB: return "Level b";
    default: return "";
    }
}
void CursorControl::enter(const CursorOptions &c) {
    line_ = c.time ? CursorLine::TimeA : c.voltage ? CursorLine::LevelA : CursorLine::None;
}
void CursorControl::sync(const CursorOptions &c) {
    if (!shown(line_, c)) enter(c);
}
void CursorControl::next(const CursorOptions &c) {
    if (line_ == CursorLine::None) { enter(c); return; }
    for (unsigned i = 0; i < 4; ++i) {
        line_ = line_ == CursorLine::LevelB ? CursorLine::TimeA : CursorLine(unsigned(line_) + 1);
        if (shown(line_, c)) return;
    }
    line_ = CursorLine::None;
}
bool CursorControl::move(CursorOptions &c, int horizontal, int vertical, bool fine, unsigned width, unsigned height) const {
    if (width < 2 || height < 2 || !shown(line_, c)) return false;
    const auto before = c;
    const int amount = fine ? 1 : 8;
    switch (line_) {
    case CursorLine::TimeA: step(c.xA, horizontal * amount, width - 1); break;
    case CursorLine::TimeB: step(c.xB, horizontal * amount, width - 1); break;
    case CursorLine::LevelA: step(c.yA, vertical * amount, height - 1); break;
    case CursorLine::LevelB: step(c.yB, vertical * amount, height - 1); break;
    default: break;
    }
    return before.xA != c.xA || before.xB != c.xB || before.yA != c.yA || before.yB != c.yB;
}
} // namespace openosc::plot
