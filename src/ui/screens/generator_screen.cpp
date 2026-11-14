#include "openosc/ui/screens/generator_screen.hpp"
#include "openosc/generator/capabilities.hpp"
#include "openosc/ui/scope/format.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
// What sets the output level: ZT703S a level profile, ZT706S the module's nominal amplitude.
void level(const generator::Settings &settings, char *output, size_t capacity) {
    if (generator::Capabilities::Levels) std::snprintf(output, capacity, "%s", generator::Levels::Names[unsigned(settings.level)]);
    else std::snprintf(output, capacity, "%u.%u V", settings.amplitudeMillivolts / 1000, settings.amplitudeMillivolts % 1000 / 100);
}
} // namespace
void GeneratorScreen::activate(const ApplicationState &) {
    auto &bar = host_.parameters();
    bar.item(0, "Wave", "--");
    bar.item(1, "Frequency", "--");
    bar.item(2, "Params", "Fixed");
    bar.item(3, "Output", "OFF");
    bar.select(1);
    host_.status("OFF", StatusTone::Neutral);
    host_.elements().text(ElementId::Reading, "--");
    host_.elements().text(ElementId::Unit, "Hz");
    host_.elements().text(ElementId::Details, "SET FREQUENCY");
    host_.elements().text(ElementId::Measurements, "");
}
void GeneratorScreen::status(const generator::Snapshot &snapshot) {
    // The ZT706S module answers asynchronously: SENDING while a frame is on the wire or waits for its
    // acknowledge, NO ACK when none came (the output state is then unknown).
    const bool error = snapshot.status == DataStatus::HardwareError, silent = snapshot.status == DataStatus::Timeout;
    const char *state = error ? "OUTPUT ERROR" : silent ? "NO ACK" : snapshot.status == DataStatus::Busy ? "SENDING"
                      : snapshot.enabled ? "ON" : "OFF";
    host_.status(state, error || silent ? StatusTone::Alert : snapshot.enabled ? StatusTone::Good : StatusTone::Neutral);
}
void GeneratorScreen::update(const generator::Snapshot &snapshot) {
    auto &elements = host_.elements();
    status(snapshot);
    const double set = snapshot.requested.frequencyHz.value();
    const unsigned divisor = set >= 1000000 ? 1000000 : set >= 1000 ? 1000 : 1;
    const char *unit = divisor == 1000000 ? "MHz" : divisor == 1000 ? "kHz" : "Hz";
    char value[48] = "--", applied[32] = "--", text[112], frequency[32], levelText[24];
    level(snapshot.requested, levelText, sizeof levelText);
    const uint64_t denominator = uint64_t(snapshot.requested.frequencyHz.denominator) * divisor;
    if (denominator) {
        const uint64_t fixed = uint64_t(snapshot.requested.frequencyHz.numerator) * 100000 / denominator;
        std::snprintf(value, sizeof value, "%lu.%05lu", static_cast<unsigned long>(fixed / 100000),
                      static_cast<unsigned long>(fixed % 100000));
    }
    elements.text(ElementId::Reading, value);
    elements.text(ElementId::Unit, unit);
    if (snapshot.applied.actualHz.valid())
        plot::Format::quantity(snapshot.applied.actualHz.value(), "Hz", applied, sizeof applied);
    std::snprintf(text, sizeof text, "Applied %s", applied);
    elements.text(ElementId::Measurements, text);
    static constexpr const char *shapes[] = {"Sine", "Square", "Triangle", "Half sine", "Pos sine", "Sawtooth"};
    const unsigned shape = unsigned(snapshot.requested.shape);
    const char *name = shape < 6 ? shapes[shape] : "Invalid";
    if (host_.theme().layout == LayoutPreset::Legacy) {
        std::snprintf(text, sizeof text, "%s / %s, uncal.", name, levelText);
        elements.text(ElementId::Measurements, text);
    }
    auto &bar = host_.parameters();
    plot::Format::quantity(set, "Hz", frequency, sizeof frequency);
    bar.item(0, "Wave", name);
    bar.item(1, "Frequency", frequency);
    std::snprintf(text, sizeof text, "%u%%", snapshot.requested.dutyPermille / 10);
    bar.item(2, "Params", snapshot.requested.shape == generator::Shape::Square ? text : levelText);
    bar.item(3, "Output", snapshot.enabled ? "ON" : "OFF");
    host_.preview().update(snapshot.requested,
        host_.theme().elements[unsigned(MeasurementMode::Generator)][unsigned(ElementId::Preview)].color);
}
} // namespace openosc::ui
