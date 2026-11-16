#include "openosc/application/controller.hpp"
#include "openosc/platform/input.hpp"
#include "openosc/generator/frequency.hpp"

namespace openosc {
void ApplicationController::openPanel(Panel panel, const ui::PropertyList &list, unsigned row) {
    panel_ = panel;
    navigator_.reset(row);
    view_.properties(list, navigator_);
}
void ApplicationController::closePanel() {
    panel_ = Panel::None;
    dataRefresh_ = true;
    view_.measurement(state_);
}
// One field at a time (PropertyNavigator): a Choice taken, a Toggle switched or a Number finished
// with MENU is Committed for the caller to apply, the panel staying open; a refused choice says why.
ApplicationController::FieldKey ApplicationController::fieldKeys(uint32_t keys, ui::PropertyList &list, uint32_t now) {
    if (!keys) return FieldKey::None;
    using Outcome = ui::PropertyNavigator::Outcome;
    const auto outcome = navigator_.keys(keys, list);
    if (outcome == Outcome::Refused && list.notice()) view_.notify(list.notice(), now);
    if (outcome == Outcome::Back) { closePanel(); return FieldKey::Closed; }
    if (outcome == Outcome::Changed) return FieldKey::Committed;
    if (outcome == Outcome::Activate) return FieldKey::Action;
    view_.properties(list, navigator_);
    return FieldKey::None;
}
// The trigger panel's committed field. Refused (a host holds the record), it shows the trigger in force.
void ApplicationController::commitTrigger(uint32_t now) {
    auto request = data_.scopeRequest();
    request.trigger = triggerEditor_.settings();
    if (!request.same(data_.scopeRequest()) && !applyEdited(request, now)) view_.notify("Finish data transfer first", now);
    triggerEditor_.begin(data_.scopeRequest().trigger);
    view_.properties(triggerEditor_, navigator_);
}
void ApplicationController::triggerKeys(uint32_t keys, uint32_t now) {
    const auto key = fieldKeys(keys, triggerEditor_, now);
    if (key == FieldKey::Action) { // Set level to 50%, from the record on show, applied at once.
        scope::FrameLease frame;
        if (!data_.acquireScopeWait(frame) || !triggerEditor_.midpoint(frame.view())) {
            view_.notify("50% needs a completed channel capture", now);
            view_.properties(triggerEditor_, navigator_);
            return;
        }
        frame.release(); // Applying restarts capture, which needs every frame lease back.
    } else if (key != FieldKey::Committed) return;
    commitTrigger(now);
}
// F3 again with the Trigger focus: the trigger source goes to the other channel the next capture
// has on (one capture source at a time); with one channel on it stays. In HOLD the next capture
// takes it, the held record and its trigger stay; a finished Single resumes as any edit does.
void ApplicationController::triggerSource(uint32_t now) {
    auto request = data_.scopeRequest();
    const unsigned other = unsigned(request.trigger.channel) ^ 1;
    if (!request.frontend.enabled[other]) {
        view_.notify(other ? "Trigger CH1: CH2 is off" : "Trigger CH2: CH1 is off", now);
        return;
    }
    request.trigger.channel = scope::Channel(other);
    if (!applyEdited(request, now)) { view_.notify("Finish data transfer first", now); return; }
    view_.notify(data_.scopeRequest().trigger.channel == scope::Channel::Ch1 ? "Trigger source CH1" : "Trigger source CH2", now);
    dataRefresh_ = true;
}
void ApplicationController::frequencyKeys(uint32_t keys, uint32_t now) {
    frequencyPanelKeys(keys, now);
    refreshSoftKeys(); // Unit on F2, Cancel on F4; closed: the screen's own cards.
}
void ApplicationController::frequencyPanelKeys(uint32_t keys, uint32_t now) {
    if (!keys) return;
    if (keys & Keypad::F4) { closePanel(); return; }
    if (switchGeneratorPanel(keys, now)) return; // F1 WAVEFORM, F3 PARAMETERS; F2 stays the unit key.
    if (keys & Keypad::Left) frequencyEditor_.move(-1);
    if (keys & Keypad::Right) frequencyEditor_.move(1);
    if (keys & Keypad::Up) frequencyEditor_.adjust(1);
    if (keys & Keypad::Down) frequencyEditor_.adjust(-1);
    if (keys & Keypad::F2) frequencyEditor_.nextUnit();
    if (keys & Keypad::Menu) {
        auto settings = data_.generator().settings();
        settings.frequencyHz = generator::Frequency::rational(frequencyEditor_.value());
        if (!applyGenerator(settings, now)) {
            view_.notify("Unsupported output frequency", now);
            return;
        }
        closePanel();
        return;
    }
    view_.frequencyEditor(frequencyEditor_);
}
} // namespace openosc
