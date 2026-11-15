#include "openosc/ui/screens/generator_screen.hpp"
#include "openosc/generator/capabilities.hpp"
#include "openosc/ui/scope/format.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
constexpr const char *Shapes[] = {"Sine", "Square", "Triangle", "Half sine", "Pos sine", "Sawtooth"};
const char *shapeName(generator::Shape shape) { return unsigned(shape) < 6 ? Shapes[unsigned(shape)] : "Invalid"; }
} // namespace
void GeneratorScreen::activate(const ApplicationState &) {
    keys_ = {};
    if (shown_) { update(snapshot_); return; } // The output as it is: never a placeholder OFF while ON.
    cards(); // Nothing shown yet: "--" and OFF.
    host_.parameters().select(1);
    host_.status("OFF", StatusTone::Neutral);
    host_.elements().text(ElementId::Reading, "--");
    host_.elements().text(ElementId::Unit, "Hz");
    host_.elements().text(ElementId::Details, "SET FREQUENCY");
    host_.elements().text(ElementId::Measurements, "");
}
void GeneratorScreen::keys(const GeneratorKeys &keys) {
    keys_ = keys;
    std::snprintf(unit_, sizeof unit_, "%s", keys.unit ? keys.unit : "");
    keys_.unit = unit_;
    cards();
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
    char value[48] = "--", applied[32] = "--", text[112];
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
    const char *name = shapeName(snapshot.requested.shape);
    if (host_.theme().layout == LayoutPreset::Legacy) {
        char described[48];
        generator::LevelText::describe(snapshot.requested, described, sizeof described);
        std::snprintf(text, sizeof text, "%s, %s", name, described);
        elements.text(ElementId::Measurements, text);
    }
    snapshot_ = snapshot; shown_ = true;
    cards();
    host_.preview().update(snapshot.requested,
        host_.theme().elements[unsigned(MeasurementMode::Generator)][unsigned(ElementId::Preview)].color);
}
// The cards: the screen's own (Main) or an open panel's keys, values from the last output.
void GeneratorScreen::cards() {
    using Panel = GeneratorKeys::Panel;
    const auto &s = snapshot_.requested;
    char frequency[32] = "--", params[24], level[24];
    if (shown_) plot::Format::quantity(s.frequencyHz.value(), "Hz", frequency, sizeof frequency);
    generator::LevelText::name(s, level, sizeof level);
    const unsigned duty = s.dutyPermille; // 50.5 % as set, not cut to 50 %.
    if (s.shape != generator::Shape::Square) std::snprintf(params, sizeof params, "%s", level);
    else if (duty % 10) std::snprintf(params, sizeof params, "%u.%u%%", duty / 10, duty % 10);
    else std::snprintf(params, sizeof params, "%u%%", duty / 10);
    auto &bar = host_.parameters();
    bar.item(0, "Wave", shown_ ? shapeName(s.shape) : "--");
    if (keys_.panel == Panel::Frequency) bar.item(1, "Unit", keys_.unit); // F2 steps Hz/kHz/MHz there.
    else bar.item(1, "Frequency", frequency);
    bar.item(2, "Params", shown_ ? params : "Fixed");
    if (keys_.panel == Panel::Main) bar.item(3, "Output", snapshot_.enabled ? "ON" : "OFF");
    else if (keys_.editing || keys_.panel == Panel::Frequency) bar.item(3, "Cancel", keys_.panel == Panel::Frequency ? "Frequency" : "Field");
    else bar.item(3, "Back", "Main");
    if (keys_.panel != Panel::Main)
        bar.select(keys_.panel == Panel::Wave ? 0 : keys_.panel == Panel::Frequency ? 1 : 2);
}
} // namespace openosc::ui
