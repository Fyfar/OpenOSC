#include "openosc/application/controller.hpp"
#include "openosc/platform/input.hpp"
#include "openosc/ui/editors/scope_actions.hpp"
#include "openosc/acquisition/timebases.hpp"
#include "openosc/ui/scope/controls.hpp"
#include "openosc/ui/scope/trace_move.hpp"
#include <algorithm>

namespace openosc {
namespace {
int axis(uint32_t keys, uint32_t negative, uint32_t positive) {
    return (keys & positive ? 1 : 0) - (keys & negative ? 1 : 0);
}
}
void ApplicationController::setScopeFocus(plot::Focus focus) {
    focus_ = focus;
    view_.waveform().cursorFocus(focus == plot::Focus::Cursor ? cursors_.line() : plot::CursorLine::None);
    view_.scopeContext(selectedChannel_, focus_);
}
// The active line has to be one that is drawn: follow every change of the cursor mode or view.
void ApplicationController::syncCursorFocus() {
    if (focus_ != plot::Focus::Cursor) return;
    const auto &options = view_.waveform().options();
    if (!plot::CursorControl::available(options)) { setScopeFocus(plot::Focus::Scale); return; }
    cursors_.sync(options.cursors);
    setScopeFocus(plot::Focus::Cursor);
}
void ApplicationController::openAnalyze(unsigned row) {
    openPanel(Panel::Analyze, ui::ScopeActions(view_.waveform().options()), row);
}
void ApplicationController::enterCursors(uint32_t now) {
    auto cursors = view_.waveform().options().cursors;
    if (!cursors.time && !cursors.voltage) cursors.time = cursors.voltage = true;
    cursors.channel = channelShown(selectedChannel_) ? selectedChannel_ : selectedChannel_ ^ 1;
    view_.waveform().cursorLines(cursors);
    cursors_.enter(cursors);
    setScopeFocus(plot::Focus::Cursor);
    queuePlotSave(now);
}
void ApplicationController::moveCursor(uint32_t keys, uint32_t now, bool fine) {
    const auto &view = view_.waveform().model().viewport();
    auto cursors = view_.waveform().options().cursors;
    if (!cursors_.move(cursors, axis(keys, Keypad::Left, Keypad::Right), axis(keys, Keypad::Up, Keypad::Down), fine, view.width, view.height)) return;
    view_.waveform().cursorLines(cursors);
    queuePlotSave(now);
}
void ApplicationController::beginHeldView() {
    if (heldViewActive_) return;
    heldView_ = view_.waveform().options();
    heldViewActive_ = true;
    heldViewRelease_ = data_.scopeHoldReleases();
}
void ApplicationController::moveTrace(uint32_t keys, uint32_t now, bool fine) {
    const bool held = data_.scopeHeld();
    if (held) beginHeldView();
    auto request = data_.scopeRequest();
    auto options = view_.waveform().options();
    const auto change = focus_ == plot::Focus::Trigger
        ? plot::TraceMove::Change{false, plot::TraceMove::pan(options, axis(keys, Keypad::Left, Keypad::Right))}
        : plot::TraceMove::apply(request, options, selectedChannel_, held, axis(keys, Keypad::Left, Keypad::Right), axis(keys, Keypad::Up, Keypad::Down), fine);
    if (change.request && !applyEdited(request, now)) view_.notify("Finish data transfer first", now);
    if (change.view) {
        view_.waveform().configure(options);
        if (!held) queuePlotSave(now);
    }
}
void ApplicationController::scopeKeys(uint32_t keys, uint32_t now, bool fine) {
    followShownChannel(); // Before any key acts on the selection.
    if (keys & Keypad::AutoRange) {
        if (autoBusy()) cancelAuto(now); else startAuto(now);
        return;
    }
    if (autoBusy()) {
        if (keys & (Keypad::F4 | Keypad::HoldSave)) {
            cancelAuto(now,keys&Keypad::HoldSave);
        }
        return;
    }
    syncCursorFocus();
    const auto openTrigger=[&] {
        triggerEditor_.begin(data_.scopeRequest().trigger);
        triggerEditor_.sources(data_.scopeRequest().frontend.enabled[0], data_.scopeRequest().frontend.enabled[1]);
        triggerEditor_.calibration(data_.scopeMetadata());
        openPanel(Panel::Trigger, triggerEditor_);
    };
    if (keys & Keypad::F3) { // As F1/F2 with their channel: a press again switches the trigger source.
        if (focus_ == plot::Focus::Trigger) { triggerSource(now); return; }
        setScopeFocus(plot::Focus::Trigger);
        return;
    }
    if (keys & Keypad::Menu) {
        if (focus_ == plot::Focus::Trigger) { openTrigger(); return; }
        if (focus_ == plot::Focus::Cursor) { openAnalyze(ui::ScopeActions::Cursors); return; }
        channelEditor_.begin(data_.scopeRequest(), view_.waveform().options(), selectedChannel_);
        openPanel(Panel::Channel, channelEditor_);
        return;
    }
    if (keys & Keypad::F4) {
        const auto &cursors = view_.waveform().options().cursors;
        if (focus_ == plot::Focus::Cursor) { // Next line, as on the factory firmware.
            cursors_.next(cursors);
            setScopeFocus(plot::Focus::Cursor);
        } else if (plot::CursorControl::available(view_.waveform().options())) enterCursors(now);
        else openAnalyze(0);
        return;
    }
    if ((keys & Keypad::HoldSave) && !data_.scopeHeld()) data_.holdScope(true);
    else if ((keys & Keypad::HoldSave) && !data_.runScope() && uiReady_) view_.notify("Cannot run: the record is in use", now);
    if (keys & Keypad::Relative) view_.scopeFooterPage(); // REL has no other Scope use; its hold is power.
    // A second F1/F2 press changes the channel only to one that is shown (followShownChannel).
    if (keys & Keypad::F1) {
        if (focus_ == plot::Focus::Scale && channelShown(selectedChannel_ ^ 1)) selectedChannel_ ^= 1;
        focus_ = plot::Focus::Scale;
    }
    if (keys & Keypad::F2) {
        if (focus_ == plot::Focus::Position && channelShown(selectedChannel_ ^ 1)) selectedChannel_ ^= 1;
        focus_ = plot::Focus::Position;
    }
    setScopeFocus(focus_);
    const uint32_t arrows = keys & (Keypad::Up | Keypad::Down | Keypad::Left | Keypad::Right);
    if (!arrows) return;
    if (focus_ == plot::Focus::Cursor) { moveCursor(arrows, now, fine); return; }
    if (focus_ == plot::Focus::Trigger && (keys & (Keypad::Up | Keypad::Down))) { // Up raises the level, as on the factory firmware.
        auto request = data_.scopeRequest();
        request.trigger.level = std::clamp<int>(request.trigger.level + ((keys & Keypad::Up) ? 1 : -1), -128, 127);
        if (!applyEdited(request, now)) view_.notify("Finish data transfer first", now);
        return;
    }
    if (focus_ == plot::Focus::Position || focus_ == plot::Focus::Trigger) { // Left/Right move the record in both, as on the factory firmware.
        moveTrace(arrows, now, fine);
        return;
    }
    if (data_.scopeHeld()) { // A held frame cannot be re-acquired: arrows scale its view instead.
        beginHeldView();
        plot::Controls controls;
        controls.options = view_.waveform().options();
        controls.channel = selectedChannel_;
        if (keys & (Keypad::Up | Keypad::Down)) controls.adjust(plot::Controls::Gain, keys & Keypad::Up ? -1 : 1); // Up widens V/div.
        auto &view = controls.options;
        if (keys & Keypad::Right) {
            if (view.mode != plot::Mode::Spectrum && view.zoomOut > 1) view.zoomOut /= 2;
            else view.zoom = std::min<uint16_t>(256, view.zoom * 2);
        }
        if (keys & Keypad::Left) {
            const auto &record = view_.waveform().model().metadata();
            const double screen = record.timebaseNs / 1e8 * record.samplesPerSecond.value();
            if (view.zoom > 1) view.zoom /= 2;
            else if (view.mode != plot::Mode::Spectrum && view.zoomOut < 64 && screen * view.zoomOut * 2 < record.samples) view.zoomOut *= 2;
        }
        view_.waveform().configure(controls.options);
        return;
    }
    auto request = data_.scopeRequest();
    auto &range = request.frontend.range[selectedChannel_];
    if ((keys & Keypad::Up) && range < 10) ++range; // Ranges ascend with V/div; the factory's Up is range + 1.
    if ((keys & Keypad::Down) && range) --range;
    if (keys & Keypad::Right) request.timebaseNs = scope::Timebases::step(request.timebaseNs, 1);
    if (keys & Keypad::Left) request.timebaseNs = scope::Timebases::step(request.timebaseNs, -1);
    plot::XyMode::constrain(view_.waveform().options(), request);
    if (!applyScope(request, now)) view_.notify("Finish data transfer first", now);
}
} // namespace openosc
