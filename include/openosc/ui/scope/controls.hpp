#pragma once
#include "openosc/ui/scope/options.hpp"
#include "openosc/ui/editors/property_list.hpp"
#include <cstddef>

namespace openosc::plot {
class Controls {
public:
    enum Row : unsigned {
        Channel,
        View,
        TraceStyle,
        GridStyle,
        Persistence,
        Average,
        PeakHold,
        Glow,
        Zoom,
        Pan,
        Visible,
        Invert,
        Gain,
        Position,
        TimeCursors,
        VoltageCursors,
        CursorAX,
        CursorBX,
        CursorAY,
        CursorBY,
        MathTrace,
        Reference,
        Clear,
        SampleReduction,
        TraceInterpolation,
        FftSource,
        FftWindow,
        FftScale,
        FftDc,
        Section,
        Count
    };
    enum class Group { View, Capture, Cursors, History, Fft, Count };
    Group group = Group::View;
    // The rows of the group: Section first, then the group's own.
    unsigned count() const;
    unsigned row(unsigned index) const;
    unsigned index(unsigned row) const;
    void open(unsigned row); // The group holding `row`.
    const char *section() const;
    static const char *groupName(unsigned group);
    Options options;
    unsigned channel = 0;
    bool enabled[2] = {true, true};
    // One row (Row) as a property: what it is, its name and value, its options, its steps.
    ui::PropertyKind kind(unsigned row) const;
    void name(unsigned row, char *output, size_t capacity) const;
    void value(unsigned row, char *output, size_t capacity) const;
    unsigned choices(unsigned row) const;
    unsigned choice(unsigned row) const;
    void choiceName(unsigned row, unsigned index, char *output, size_t capacity) const;
    void adjust(unsigned row, int direction, bool fine = true);
    int32_t number(unsigned row) const;
    void setNumber(unsigned row, int32_t value);
};
} // namespace openosc::plot
