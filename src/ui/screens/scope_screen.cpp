#include "openosc/ui/screens/scope_screen.hpp"
#include "openosc/ui/scope/format.hpp"
#include "openosc/ui/scope/display_scale.hpp"
#include "openosc/acquisition/readouts.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::ui {
void ScopeScreen::activate(const ApplicationState &state) {
    measurements_=state.scope.measurements.valid() ? state.scope.measurements : scope::ReadoutSelection{};
    host_.scopeFooter().selection(measurements_);
    countersValid_=false; readoutsPainted_=false;
    valid_ = false;
    status_ = "WAITING FOR DATA";
    automatic_=calibrating_=false;
    host_.status(status_, StatusTone::Warning);
    // Closing a panel reactivates this screen: a trace still on display keeps its labels.
    const bool traced=host_.waveform().model().hasFrame();
    host_.elements().text(ElementId::Measurements, traced ? "" : "No completed capture");
    if (!traced) {
        host_.scopeFooter().clear();
        scope::FrameMetadata next;
        next.timebaseNs = state.scope.timebaseNs;
        for (unsigned c = 0; c < 2; ++c) { next.channels[c].millivoltsPerDivision = state.scope.millivoltsPerDivision[c]; next.channels[c].calibrated = true; }
        next_ = next;
    }
    cards();
}
void ScopeScreen::settings(const scope::FrameMetadata &next, unsigned channel) {
    next_ = next;
    channel_ = channel < 2 ? channel : 0;
    cards();
}
void ScopeScreen::context(unsigned channel, plot::Focus focus) {
    if (channel_ != channel) valid_ = false;
    channel_ = channel < 2 ? channel : 0;
    focus_ = focus;
    cards();
}
void ScopeScreen::generator(const generator::Snapshot &output) {
    char text[48] = "", frequency[24];
    if (output.enabled && output.applied.actualHz.valid() && plot::Format::quantity(output.applied.actualHz.value(), "Hz", frequency, sizeof frequency, 3)) {
        const unsigned shape = unsigned(output.requested.shape);
        std::snprintf(text, sizeof text, "GEN %s %s", shape < 6 ? generator::ShapeNames[shape] : "?", frequency);
    }
    host_.waveform().note(text, host_.theme().generator);
}
void ScopeScreen::cards() {
    auto &bar = host_.parameters();
    const auto &widget = host_.waveform();
    const auto &model = widget.model();
    const bool framed = model.hasFrame();
    const auto &shown = framed ? model.metadata() : next_;
    const auto &selected = shown.channels[channel_];
    char scale[24], time[24], value[64], title[64], details[160];
    const auto scaleValue = plot::DisplayScale::channel(selected, framed ? model.options().channels[channel_] : plot::ChannelOptions{});
    plot::Format::quantity(scaleValue.perDivision, scaleValue.unit, scale, sizeof scale, 3); // The strip uses the same precision.
    const bool spectrum = widget.options().mode == plot::Mode::Spectrum;
    plot::Format::quantity(framed && !spectrum ? plot::DisplayScale::secondsPerDivision(model.viewport(), shown) : shown.timebaseNs / 1e9, "s", time, sizeof time, 3);
    std::snprintf(value, sizeof value, "%s %s", scale, time); // F1 is VOL/TIME, as on the factory firmware.
    char probe[24], next[48] = "";
    plot::DisplayScale::probe(selected, probe, sizeof probe);
    if (framed && plot::DisplayScale::nextDiffers(shown, next_, channel_)) {
        const auto scaleNext = plot::DisplayScale::channel(next_.channels[channel_], plot::ChannelOptions{});
        char volts[16], seconds[16];
        plot::Format::quantity(scaleNext.perDivision, scaleNext.unit, volts, sizeof volts);
        plot::Format::quantity(next_.timebaseNs / 1e9, "s", seconds, sizeof seconds, 3);
        std::snprintf(next, sizeof next, "Next %s %s", volts, seconds);
    }
    if (*next) std::snprintf(title, sizeof title, "%s", next);
    else std::snprintf(title, sizeof title, "CH%u %s %s", channel_ + 1, plot::DisplayScale::coupling(selected), probe);
    bar.item(0, title, value);
    std::snprintf(value, sizeof value, "CH%u", channel_ + 1);
    bar.item(1, "Position", value);
    std::snprintf(value, sizeof value, "CH%u", unsigned(model.triggerChannel()) + 1);
    bar.item(2, "Trigger", value);
    if (focus_ == plot::Focus::Cursor && model.cursorFocus() != plot::CursorLine::None)
        bar.item(3, "Cursor", plot::CursorControl::lineName(model.cursorFocus(), spectrum));
    else
        bar.item(3, "Analyze", widget.options().mode == plot::Mode::XY ? "XY" : spectrum ? "FFT" : widget.options().mode == plot::Mode::SpectrumOverlay ? "Y-T+FFT" : "Display");
    if (!host_.overlayActive()) bar.select(unsigned(focus_));
    std::snprintf(details, sizeof details, "CH%u %s %s  %s %s%s%s", channel_ + 1, plot::DisplayScale::coupling(selected), probe, scale, time, *next ? "  " : "", next);
    host_.elements().text(ElementId::Details, details);
}
void ScopeScreen::update(const scope::FrameView &frame, bool held, unsigned selected, DataStatus health) {
    if (!frame.valid() || selected > 1 || host_.overlayActive()) return;
    auto &widget = host_.waveform();
    widget.held(held);
    widget.setFrame(frame);
    status(health, held);

    if (host_.theme().layout != LayoutPreset::Legacy) {
        host_.scopeFooter().frame(widget.model());
        cards();
    }
    if (valid_ && held_ == held && channel_ == selected) return;
    valid_ = true;
    held_ = held;
    channel_ = selected;
    if (host_.theme().layout != LayoutPreset::Legacy) host_.elements().text(ElementId::Measurements,"");
}
void ScopeScreen::nextFooterPage() {
    if (host_.theme().layout != LayoutPreset::Legacy && !host_.overlayActive()) host_.scopeFooter().nextPage();
}
// Values computed beside the waveform (ReadoutScheduler): never recomputed while painting a frame.
bool ScopeScreen::readouts(const scope::ReadoutResult &result, const ReadoutState &state) {
    if (!valid_ || host_.overlayActive()) return false;
    if (host_.theme().layout != LayoutPreset::Legacy) {
        host_.scopeFooter().readouts(result, state);
        return readoutsPainted_ = true;
    }
    char frequency[32] = "--", amplitude[32] = "--", text[160];
    double value;
    const auto &values = result.values[channel_];
    ReadoutText::Presentation shown; // The footer's reading of the same value: its mark and digit limit.
    if (result.computed[channel_] && values.value(scope::Measurement::Frequency, value)) {
        shown = ReadoutText::presentation(values, scope::Measurement::Frequency);
        plot::Format::quantity(value, "Hz", frequency, sizeof frequency, shown.limit ? shown.limit : 4);
    }
    if (result.computed[channel_] && values.value(scope::Measurement::PeakToPeak, value))
        plot::Format::quantity(value, "V", amplitude, sizeof amplitude);
    std::snprintf(text, sizeof text, "CH%u %s Vpp %s  f%s %s", channel_ + 1, ReadoutText::basisName(result.basis), amplitude, shown.mark,
                  frequency);
    host_.elements().text(ElementId::Measurements, text);
    return readoutsPainted_ = true;
}
} // namespace openosc::ui
