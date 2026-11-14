#pragma once
#include "openosc/ui/components/elements.hpp"
#include "openosc/measurement/dmm/session.hpp"
#include "openosc/ui/components/meter_bar.hpp"

namespace openosc::ui {
// Range/REL reference, secondary reading and range mode on one row; below it MIN, MAX and AVG in
// three columns: their names in one row, the values under them on one baseline, in one font (the
// largest of 14 and 12 in which all three fit their column with the sign column reserved).
class MeterSummary {
public:
    static constexpr int Width = 296, Columns = 3, ColumnGap = 4, ColumnWidth = (Width - (Columns - 1) * ColumnGap) / Columns;
    static constexpr int NameY = 28, ValueY = 41, ValueHeight = 16;
    static constexpr unsigned ValueFont = 14;
    static constexpr int columnX(unsigned column) { return int(column) * (ColumnWidth + ColumnGap); }
    void create(lv_obj_t *parent);
    void visible(bool value);
    // Names, the three values, the range/secondary texts, the separator and the bar's base (MeterLook).
    void palette(const MeterLook &look);
    // The analog dial shows the magnitude: no bar.
    void bar(bool shown) { bar_.visible(shown); }
    // The analog layout: the reference, range mode and secondary reading on one 14 px meta row at
    // the summary's top (right of the function sign, CompactMetaX on), MIN/MAX/AVG on one 14 px
    // row `statsY` lower, each name + value group centred in its third; no separator. Off: the
    // digital arrangement above.
    void compact(bool on, int statsY);
    static constexpr int CompactMetaX = 60, CompactRow = 14, CompactNameGap = 4;
    static constexpr unsigned CompactFont = 12;
    static constexpr int BarTop = 63, Height = 89; // The bar's rows: BarTop..Height.
    void update(const dmm::Result &result, const dmm::Statistics *statistics, uint32_t accent,
                double reference = 0, bool relative = false, bool peerRelative = false);
    void bar(const dmm::Result &result, DataStatus status, uint32_t accent,
             const dmm::RelativeReading *relative = nullptr) { bar_.update(result,status,accent,relative); }
    // MIN, MAX, AVG (tests).
    lv_obj_t *statistic(unsigned column) const { return values_[column]; }
    lv_obj_t *statisticName(unsigned column) const { return names_[column]; }
    lv_obj_t *rangeMode() const { return range_; } // AUTO / MANUAL (tests).

private:
    void arrange();
    void centre(unsigned column);
    static int textWidth(lv_obj_t *label); // Its text in its own font (the role's typeface).
    MeterLook look_{};
    bool compact_ = false;
    int statsY_ = 0;
    lv_obj_t *root_ = nullptr, *range_ = nullptr, *secondary_ = nullptr, *separator_ = nullptr;
    lv_obj_t *names_[Columns] = {}, *values_[Columns] = {};
    MeterBar bar_;
    lv_obj_t *reference_ = nullptr;
};
} // namespace openosc::ui
