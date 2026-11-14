#include "openosc/ui/editors/generator_editor.hpp"
#include "openosc/generator/frequency.hpp"
#include "openosc/ui/scope/format.hpp"
#include <algorithm>
#include <cstdio>

namespace openosc::ui {
namespace {
using Caps = generator::Capabilities;
using Row = GeneratorEditor::Row;
constexpr Row WaveRows[] = {Row::Shape, Row::Duty, Caps::Levels ? Row::Level : Row::Amplitude, Row::Frequency};
constexpr Row DacParameters[] = {Row::Duty, Row::Level, Row::MinimumCode, Row::MaximumCode, Row::Preset, Row::Frequency};
constexpr Row ModuleParameters[] = {Row::Duty, Row::Amplitude, Row::Preset, Row::Frequency};
constexpr uint32_t Presets[] = {10, 50, 100, 500, 1000, 2000, 5000};
constexpr unsigned PresetCount = sizeof Presets / sizeof *Presets;
}
unsigned GeneratorEditor::count() const {
    if (wave_) return sizeof WaveRows / sizeof *WaveRows;
    return Caps::Levels ? sizeof DacParameters / sizeof *DacParameters : sizeof ModuleParameters / sizeof *ModuleParameters;
}
GeneratorEditor::Row GeneratorEditor::row(unsigned index) const {
    const Row *rows = wave_ ? WaveRows : Caps::Levels ? DacParameters : ModuleParameters;
    return index < count() ? rows[index] : Row::Frequency;
}
PropertyKind GeneratorEditor::kind(unsigned index) const {
    switch (row(index)) {
    case Row::Shape: case Row::Level: case Row::Preset: return PropertyKind::Choice;
    case Row::Duty: return Caps::duty(draft_.settings.shape) ? PropertyKind::Number : PropertyKind::Info;
    case Row::Amplitude: case Row::MinimumCode: case Row::MaximumCode: return PropertyKind::Number;
    case Row::Frequency: return PropertyKind::Info; // Set in the frequency editor.
    }
    return PropertyKind::Info;
}
void GeneratorEditor::name(unsigned index, char *output, size_t capacity) const {
    static constexpr const char *names[] = {"Wave", "Duty", "Level", "Amplitude", "DAC min", "DAC max", "Preset", "Frequency"};
    std::snprintf(output, capacity, "%s", names[unsigned(row(index))]);
}
unsigned GeneratorEditor::options(unsigned index) const {
    switch (row(index)) {
    case Row::Shape: return 6;
    case Row::Level: return 4;
    case Row::Preset: return PresetCount;
    default: return 0;
    }
}
unsigned GeneratorEditor::option(unsigned index) const {
    switch (row(index)) {
    case Row::Shape: return unsigned(draft_.settings.shape);
    case Row::Level: return unsigned(draft_.settings.level);
    case Row::Preset: // A frequency off the presets is none of them until one is chosen.
        for (unsigned i = 0; i < PresetCount; ++i) if (draft_.settings.frequencyHz.value() == Presets[i]) return i;
        return PresetCount;
    default: return 0;
    }
}
void GeneratorEditor::optionName(unsigned index, unsigned option, char *output, size_t capacity) const {
    switch (row(index)) {
    case Row::Shape: std::snprintf(output, capacity, "%s", generator::ShapeNames[option % 6]); break;
    case Row::Level: std::snprintf(output, capacity, "%s", generator::Levels::Names[option % 4]); break;
    case Row::Preset: plot::Format::quantity(Presets[option % PresetCount], "Hz", output, capacity); break;
    default: if (capacity) output[0] = 0; break;
    }
}
void GeneratorEditor::limit() {
    const uint64_t maximum = Caps::maximumMillihertz(draft_.settings.shape, draft_.settings.level);
    if (generator::Frequency::millihertz(draft_.settings.frequencyHz) > maximum) {
        draft_.settings.frequencyHz = generator::Frequency::rational(maximum); draft_.clamped = true;
    }
    if (Caps::Amplitude) {
        const uint16_t top = Caps::amplitudeLimitMillivolts(draft_.settings.shape, generator::Frequency::millihertz(draft_.settings.frequencyHz));
        if (draft_.settings.amplitudeMillivolts > top) { draft_.settings.amplitudeMillivolts = top; draft_.amplitudeClamped = true; }
    }
}
bool GeneratorEditor::choose(unsigned index, unsigned option) {
    if (option >= options(index)) return false;
    switch (row(index)) {
    case Row::Shape: draft_.settings.shape = generator::Shape(option); break;
    case Row::Level: draft_.settings.level = generator::Level(option); break;
    case Row::Preset: draft_.settings.frequencyHz = {Presets[option], 1}; break;
    default: return false;
    }
    limit();
    return true;
}
int32_t GeneratorEditor::number(unsigned index) const {
    switch (row(index)) {
    case Row::Duty: return draft_.settings.dutyPermille;
    case Row::Amplitude: return draft_.settings.amplitudeMillivolts;
    case Row::MinimumCode: case Row::MaximumCode: {
        const auto codes = generator::Levels::codes(draft_.settings);
        return row(index) == Row::MinimumCode ? codes.minimum : codes.maximum;
    }
    default: return 0;
    }
}
void GeneratorEditor::adjust(unsigned index, int direction) {
    if (!direction) return;
    const int step = direction > 0 ? 1 : -1;
    switch (row(index)) {
    case Row::Duty:
        if (Caps::duty(draft_.settings.shape))
            draft_.settings.dutyPermille = uint16_t(std::clamp<int>(int(draft_.settings.dutyPermille) + step * Caps::DutyStepPermille,
                                                              Caps::MinimumDutyPermille, Caps::MaximumDutyPermille));
        break;
    case Row::Amplitude:
        if (Caps::Amplitude)
            draft_.settings.amplitudeMillivolts = uint16_t(std::clamp<int>(int(draft_.settings.amplitudeMillivolts) + step * Caps::AmplitudeStepMillivolts, 0,
                Caps::amplitudeLimitMillivolts(draft_.settings.shape, generator::Frequency::millihertz(draft_.settings.frequencyHz))));
        break;
    case Row::MinimumCode: case Row::MaximumCode: {
        const auto codes = generator::Levels::codes(draft_.settings);
        draft_.settings.minimumCode = codes.minimum; draft_.settings.maximumCode = codes.maximum;
        draft_.settings.level = generator::Level::Custom;
        if (row(index) == Row::MinimumCode) draft_.settings.minimumCode = uint16_t(std::clamp<int>(int(draft_.settings.minimumCode) + step * 25, 0, draft_.settings.maximumCode));
        else draft_.settings.maximumCode = uint16_t(std::clamp<int>(int(draft_.settings.maximumCode) + step * 25, draft_.settings.minimumCode, 4095));
        break;
    }
    default: break;
    }
    limit();
}
void GeneratorEditor::value(unsigned index, char *output, size_t capacity) const {
    char requested[24], maximum[24];
    switch (row(index)) {
    case Row::Duty:
        if (Caps::duty(draft_.settings.shape)) std::snprintf(output, capacity, "%u.%u%%", draft_.settings.dutyPermille / 10, draft_.settings.dutyPermille % 10);
        else std::snprintf(output, capacity, "Not available");
        return;
    case Row::Amplitude: { // Nominal: the module's output voltage is not calibrated.
        const unsigned top = Caps::amplitudeLimitMillivolts(draft_.settings.shape, generator::Frequency::millihertz(draft_.settings.frequencyHz));
        std::snprintf(output, capacity, "%u.%u V (max %u.%u) nom.", draft_.settings.amplitudeMillivolts / 1000,
                      draft_.settings.amplitudeMillivolts % 1000 / 100, top / 1000, top % 1000 / 100);
        return;
    }
    case Row::MinimumCode: case Row::MaximumCode:
        std::snprintf(output, capacity, "%ld codes", long(number(index)));
        return;
    case Row::Preset:
        plot::Format::quantity(draft_.settings.frequencyHz.value(), "Hz", output, capacity);
        return;
    case Row::Frequency:
        plot::Format::quantity(draft_.settings.frequencyHz.value(), "Hz", requested, sizeof requested);
        plot::Format::quantity(Caps::maximumMillihertz(draft_.settings.shape, draft_.settings.level) / 1000.0, "Hz", maximum, sizeof maximum);
        std::snprintf(output, capacity, "%s (max %s)", requested, maximum);
        return;
    default: PropertyList::value(index, output, capacity); return;
    }
}
} // namespace openosc::ui
