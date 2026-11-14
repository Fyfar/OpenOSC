#pragma once
#include "openosc/board/current.hpp"
#include "openosc/ui/editors/property_list.hpp"
#include "openosc/measurement/dmm/result.hpp"
#include "openosc/ui/format/meter.hpp"

namespace openosc::ui {
class MeterProperties final : public PropertyList {
public:
    enum Row { Range, Display, Colors, Digits, Appearance, Statistics, Relative, ResetStatistics, Logging, SerialOutput, Count };
    static constexpr unsigned VisibleRows = board::Current::Features.dmmSerialOutput ? Count : SerialOutput;
    explicit MeterProperties(const dmm::Result &result, unsigned display = 0, bool statistics = true,
                             bool relative = false, bool logging = false, bool serial=false, bool jackBusy=false,
                             unsigned face = 0, unsigned palette = 0)
        : result_(result), display_(display), face_(face), palette_(palette), statistics_(statistics), relative_(relative),
          logging_(logging),serial_(serial),jackBusy_(jackBusy) {}
    unsigned display() const { return display_; }
    unsigned face() const { return face_; }
    unsigned palette() const { return palette_; }
    bool statistics() const { return statistics_; }
    bool relative() const { return relative_; }
    bool automatic() const { return result_.range == dmm::RangeMode::Auto; }
    bool logging() const { return logging_; }
    bool serial() const { return serial_; }
    const char *notice() const override { return MeterText::function(result_); }
    const char *title() const override { return "MEASUREMENT"; }
    unsigned count() const override { return VisibleRows; }
    PropertyKind kind(unsigned row) const override;
    void name(unsigned row, char *output, size_t capacity) const override;
    void value(unsigned row, char *output, size_t capacity) const override;
    unsigned options(unsigned row) const override;
    unsigned option(unsigned row) const override;
    void optionName(unsigned row, unsigned index, char *output, size_t capacity) const override;
    bool choose(unsigned row, unsigned index) override;
private:
    dmm::Result result_;
    unsigned display_, face_, palette_;
    bool statistics_, relative_, logging_,serial_,jackBusy_;
};
} // namespace openosc::ui
