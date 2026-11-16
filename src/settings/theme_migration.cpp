#include "openosc/settings/theme_migration.hpp"

namespace openosc {
namespace {
struct Box { int16_t x, y, width, height; };
// The Scope waveform and footer of every earlier stock Cards/Compact layout (theme_defaults history).
constexpr Box StockWaveforms[] = {{10, 58, 300, 143}, {10, 58, 300, 131}, {10, 58, 300, 148}, {10, 58, 300, 137},
                                  {10, 43, 300, 158}, {10, 43, 300, 146}, {10, 42, 300, 153},
                                  {10, 58, 300, 161}, {10, 42, 300, 177}}; // The last two: before the marker margins.
constexpr Box StockFooters[] = {{8, 207, 304, 32}, {8, 195, 304, 44}, {8, 209, 304, 29}, {8, 196, 304, 42},
                                {0, 220, 320, 20}}; // The one-row footer (ScopeFooterLayout).
bool at(const ElementStyle &e, const Box &b) { return e.x == b.x && e.y == b.y && e.width == b.width && e.height == b.height; }
template <size_t N> bool stock(const ElementStyle &e, const Box (&boxes)[N]) {
    for (const auto &b : boxes)
        if (at(e, b)) return true;
    return false;
}
void geometry(ElementStyle &e, const ElementStyle &from) { e.x = from.x; e.y = from.y; e.width = from.width; e.height = from.height; }
// The DMM reading/unit/details/summary of earlier stock layouts: {reading, unit, details, summary}, fonts.
struct MeterBody { Box reading; uint8_t readingFont; Box unit, details, summary; };
constexpr MeterBody StockMeters[] = {
    {{12, 84, 240, 60}, 56, {252, 103, 60, 32}, {12, 66, 296, 18}, {12, 149, 296, 89}}, // Cards/Compact, 56 px.
    {{12, 84, 240, 57}, 40, {252, 103, 60, 32}, {12, 66, 296, 18}, {8, 209, 304, 29}},  // Cards/Compact, 40 px.
    {{25, 87, 230, 48}, 32, {240, 102, 70, 32}, {12, 66, 296, 18}, {10, 55, 300, 18}},  // Legacy.
};
} // namespace
bool ThemeMigration::legacyStock(unsigned mode, unsigned element, const ElementStyle &s, const ElementStyle &current) {
    if (at(s, {current.x, current.y, current.width, current.height})) return true;
    const auto &legacy = StockMeters[2];
    if (mode == 1 && element == unsigned(ElementId::Reading)) return at(s, legacy.reading);
    if (mode == 1 && element == unsigned(ElementId::Unit)) return at(s, legacy.unit);
    if (mode == 1 && element == unsigned(ElementId::Measurements)) return at(s, legacy.summary);
    return mode == 0 && element == unsigned(ElementId::Waveform) && at(s, {10, 38, 300, 152});
}
// Not inlined: its stock theme (2.3 KiB) stays out of the caller's frame.
__attribute__((noinline)) bool ThemeMigration::adoptStockMeter(Theme &theme) {
    auto &meter = theme.elements[unsigned(MeasurementMode::Multimeter)];
    auto &reading = meter[unsigned(ElementId::Reading)], &unit = meter[unsigned(ElementId::Unit)];
    auto &details = meter[unsigned(ElementId::Details)], &summary = meter[unsigned(ElementId::Measurements)];
    const auto stock = Theme::defaults(theme.layout);
    const auto &defaults = stock.elements[unsigned(MeasurementMode::Multimeter)];
    for (const auto &old : StockMeters) {
        if (!at(reading, old.reading) || reading.font != old.readingFont || !at(unit, old.unit) || unit.font != 24) continue;
        const auto adopt = [&](ElementStyle &e, ElementId id) {
            const auto &next = defaults[unsigned(id)];
            geometry(e, next);
            e.font = next.font;
        };
        adopt(reading, ElementId::Reading);
        adopt(unit, ElementId::Unit);
        // The details line was hidden in Legacy; the DMM body shows the function row there.
        if (at(details, old.details) && details.font == 14) { adopt(details, ElementId::Details); details.visible = true; }
        if (at(summary, old.summary)) adopt(summary, ElementId::Measurements);
        return true;
    }
    return false;
}
__attribute__((noinline)) bool ThemeMigration::adoptStockScope(Theme &theme) {
    if (theme.layout == LayoutPreset::Legacy) return false;
    auto &scope = theme.elements[0];
    auto &waveform = scope[unsigned(ElementId::Waveform)], &footer = scope[unsigned(ElementId::Measurements)];
    if (!stock(waveform, StockWaveforms) || !stock(footer, StockFooters)) return false;
    const auto defaults = Theme::defaults(theme.layout);
    geometry(waveform, defaults.elements[0][unsigned(ElementId::Waveform)]);
    geometry(footer, defaults.elements[0][unsigned(ElementId::Measurements)]);
    return true;
}
} // namespace openosc
