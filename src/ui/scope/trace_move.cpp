#include "openosc/ui/scope/trace_move.hpp"
#include <algorithm>

namespace openosc::plot {
namespace {
constexpr int AnalogFine = 1, AnalogCoarse = 4, DigitalFine = 7, DigitalCoarse = 25, PanStep = 25;
bool shiftAnalog(ScopeRequest &request, unsigned channel, int amount) {
    int &position = request.frontend.position[channel];
    const int next = std::clamp(position + amount, 0, 160);
    const bool changed = next != position;
    position = next;
    return changed;
}
bool shiftDigital(Options &options, unsigned channel, int amount) {
    auto &position = options.channels[channel].positionPermille;
    const auto next = int16_t(std::clamp<int>(position + amount, -1000, 2000));
    const bool changed = next != position;
    position = next;
    return changed;
}
} // namespace
bool TraceMove::pan(Options &options, int horizontal) {
    const auto next = int16_t(std::clamp<int>(options.pan + horizontal * PanStep, -1000, 1000));
    const bool changed = next != options.pan;
    options.pan = next;
    return changed;
}
TraceMove::Change TraceMove::apply(ScopeRequest &request, Options &options, unsigned channel, bool held, int horizontal, int vertical, bool fine) {
    Change change;
    const int analog = fine ? AnalogFine : AnalogCoarse, digital = fine ? DigitalFine : DigitalCoarse;
    if (options.mode == Mode::XY) { // CH1 is the X axis and CH2 the Y axis; a lower screen line is a lower voltage.
        if (held) {
            change.view = shiftDigital(options, 0, horizontal * digital);
            change.view = shiftDigital(options, 1, vertical * digital) || change.view;
        } else {
            change.request = shiftAnalog(request, 0, -horizontal * analog);
            change.request = shiftAnalog(request, 1, vertical * analog) || change.request;
        }
        return change;
    }
    change.view = pan(options, horizontal);
    if (held) change.view = shiftDigital(options, channel, vertical * digital) || change.view;
    else change.request = shiftAnalog(request, channel, vertical * analog);
    return change;
}
void XyMode::enter(ScopeRequest &request, Options &options) {
    request.trigger.mode = scope::TriggerMode::Auto;
    request.timebaseNs = std::min(request.timebaseNs, MaximumTimebaseNs);
    options.persistenceMs = 0;
}
void XyMode::constrain(const Options &options, ScopeRequest &request) {
    if (options.mode == Mode::XY) request.timebaseNs = std::min(request.timebaseNs, MaximumTimebaseNs);
}
} // namespace openosc::plot
