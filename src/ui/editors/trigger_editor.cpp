#include "openosc/ui/editors/trigger_editor.hpp"
#include "openosc/ui/scope/format.hpp"
#include <algorithm>
#include <cstdio>

namespace openosc::ui {
bool TriggerEditor::midpoint(const scope::FrameView &frame) {
    scope::SignalBand bands[2];
    const auto &band=bands[unsigned(settings_.channel)];
    if (!scope::SignalBands::measure(frame,bands) || !band.valid) return false;
    settings_.level=int16_t((int(band.low)+band.high)/2);
    return true;
}
PropertyKind TriggerEditor::kind(unsigned row) const {
    switch (row) {
    case Level: case Pretrigger: return PropertyKind::Number;
    case Midpoint: return PropertyKind::Action;
    case Reject: return PropertyKind::Toggle;
    default: return PropertyKind::Choice;
    }
}
void TriggerEditor::name(unsigned row, char *output, size_t capacity) const {
    static constexpr const char *names[] = {"Source", "Edge", "Level", "Mode", "Pretrigger", "Noise reject", "Set level to 50%"};
    if (row == Level && !calibration_[unsigned(settings_.channel)].calibrated) std::snprintf(output, capacity, "Level (raw)");
    else std::snprintf(output, capacity, "%s", row < Fields ? names[row] : "");
}
unsigned TriggerEditor::option(unsigned row) const {
    return row == Source ? unsigned(settings_.channel) : row == Slope ? unsigned(settings_.edge) : row == Mode ? unsigned(settings_.mode) :
           row == Reject ? unsigned(settings_.reject) : 0;
}
void TriggerEditor::optionName(unsigned row, unsigned index, char *output, size_t capacity) const {
    static constexpr const char *modes[] = {"Auto", "Normal", "Single"};
    if (row == Source) std::snprintf(output, capacity, "CH%u", index + 1);
    else if (row == Slope) std::snprintf(output, capacity, "%s", index ? "Falling" : "Rising");
    else if (row == Mode && index < 3) std::snprintf(output, capacity, "%s", modes[index]);
    else PropertyList::optionName(row, index, output, capacity); // Noise reject: Off/On.
}
void TriggerEditor::value(unsigned row, char *output, size_t capacity) const {
    if (row == Level) {
        const auto &cal = calibration_[unsigned(settings_.channel)];
        if (cal.calibrated)
            plot::Format::quantity((settings_.level-cal.zeroCode)*cal.voltsPerCode*cal.probeNumerator/cal.probeDenominator,"V",output,capacity);
        else std::snprintf(output,capacity,"%d cnt",settings_.level);
    } else if (row == Pretrigger) std::snprintf(output,capacity,"%u.%u%%",settings_.position/10,settings_.position%10);
    else PropertyList::value(row, output, capacity);
}
void TriggerEditor::adjust(unsigned row, int direction) {
    if (!direction) return;
    switch (row) {
    case Source:
        if (enabled_[unsigned(settings_.channel)^1]) settings_.channel = scope::Channel(unsigned(settings_.channel)^1);
        break;
    case Slope: settings_.edge = scope::Edge(unsigned(settings_.edge) ^ 1); break;
    case Level: settings_.level = std::clamp<int>(settings_.level + (direction > 0 ? 1 : -1), -128, 127); break;
    case Mode: settings_.mode = scope::TriggerMode((unsigned(settings_.mode) + 3 + (direction > 0 ? 1 : -1)) % 3); break;
    case Pretrigger: settings_.position=std::clamp<int>(int(settings_.position)+(direction>0 ? 25 : -25),0,1000); break;
    case Reject: settings_.reject = !settings_.reject; break;
    default: break;
    }
}
} // namespace openosc::ui
