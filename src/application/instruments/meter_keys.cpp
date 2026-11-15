#include "openosc/application/controller.hpp"
#include "openosc/platform/input.hpp"
#include "openosc/ui/editors/meter_properties.hpp"

namespace openosc {
void ApplicationController::meterKeys(uint32_t keys, uint32_t now) {
    // AUTO/RANGE short press: the factory's 20 20 (703S 08044274, 706S 08044BE4): AUTO -> Manual, then
    // the next range. Its hold returns to AUTO (onKeys).
    if ((keys & Keypad::AutoRange) && !meterCommand(data_.advanceDmmRange())) view_.notify("Range command busy", now);
    if ((keys & (Keypad::Up | Keypad::Down)) && data_.meter().latest().range == dmm::RangeMode::Manual &&
        !meterCommand(data_.stepDmmRange(keys & Keypad::Up ? 1 : -1)))
        view_.notify("Range command busy", now);
    if (keys & (Keypad::Left | Keypad::Right)) {
        constexpr uint32_t n = MeterPreferences::Displays;
        prefs_.meter.display = (prefs_.meter.display + (keys & Keypad::Right ? 1 : n - 1)) % n;
        changed(now); dataRefresh_ = true;
    }
    if (keys & Keypad::Relative) {
        const bool enabled = !(data_.meter().relative().active() || data_.meter().latest().peerRelative);
        if (!data_.relativeMeter(enabled)) view_.notify("REL needs a valid current reading", now);
        dataRefresh_ = true;
    }
    if (keys & Keypad::Menu) {
        meterEditor_ = meterProperties();
        openPanel(Panel::Meter, meterEditor_);
        return;
    }
    if ((keys & Keypad::HoldSave) && !data_.holdMeter(!data_.meter().held())) view_.notify("HOLD needs a valid reading", now);
    dmm::CommandGroup group = dmm::CommandGroup::Voltage;
    if (keys & Keypad::F2) group = dmm::CommandGroup::ResistanceGroup;
    if (keys & Keypad::F3) group = dmm::CommandGroup::Current;
    if (keys & Keypad::F4)
        group = data_.meter().latest().currentInput ? dmm::CommandGroup::Milliamps : dmm::CommandGroup::Millivolts;
    if ((keys & (Keypad::F1 | Keypad::F2 | Keypad::F3 | Keypad::F4)) && !meterCommand(data_.sendDmmGroup(group)))
        view_.notify("Measurement command busy", now);
}
// AUTO/RANGE hold: the factory's 22 22 after ~2 s (703S 0804423C, 706S 08044BAC): AUTO again.
void ApplicationController::meterRangeHold(uint32_t now) {
    if (!meterCommand(data_.setDmmAuto(true))) view_.notify("Range command busy", now);
}
// The DMM panel as the instrument and the preferences have it now.
ui::MeterProperties ApplicationController::meterProperties() const {
    const auto &meter = data_.meter();
    return ui::MeterProperties(meter.displayed(),prefs_.meter.display,prefs_.meter.statistics,
        meter.relative().active() || meter.latest().peerRelative,
        data_.meterLogger().active(),data_.meterSerial().enabled(),data_.generator().enabled(),
        prefs_.meter.face, prefs_.meter.palette, &themes_.theme());
}
// The open DMM panel after the instrument changed on its own (output stopped, logging ended, new function).
void ApplicationController::refreshMeterPanel() {
    if (panel_ != Panel::Meter) return;
    meterEditor_ = meterProperties();
    view_.properties(meterEditor_, navigator_);
}
void ApplicationController::meterPanelKeys(uint32_t keys, uint32_t now) {
    if (!keys) return;
    using Outcome = ui::PropertyNavigator::Outcome;
    const auto outcome = navigator_.keys(keys, meterEditor_);
    if (outcome == Outcome::Back) { closePanel(); return; }
    if (outcome == Outcome::Open && navigator_.row() == ui::MeterProperties::Layout) { openMeterLayout(); return; }
    if (outcome == Outcome::Open) { // Appearance: its own list, over the base the panel shows.
        // The theme in force and the DMM cards' colour, as MeterScreen styles with: Default/Base show the real base.
        const auto &theme = themes_.theme();
        const uint32_t cards = theme.elements[unsigned(MeasurementMode::Multimeter)][unsigned(ElementId::Parameters)].color;
        appearanceEditor_ = ui::MeterAppearanceProperties(prefs_.meter.palette, prefs_.meter.face, prefs_.meter.appearance,
                                                          appearanceEditor_.element(), &theme, cards);
        openPanel(Panel::MeterAppearance, appearanceEditor_);
        return;
    }
    if (outcome == Outcome::Activate) data_.meter().resetStatistics(); // The one action row.
    if (outcome == Outcome::Changed) meterChanged(now);
    view_.properties(meterEditor_, navigator_);
}
// Appearance: every committed choice and reset is taken at once and saved; the reading, range,
// REL, HOLD and statistics are untouched (only MeterScreen's style changes). Back: the DMM panel.
void ApplicationController::meterAppearanceKeys(uint32_t keys, uint32_t now) {
    if (!keys) return;
    using Outcome = ui::PropertyNavigator::Outcome;
    using Editor = ui::MeterAppearanceProperties;
    const auto outcome = navigator_.keys(keys, appearanceEditor_);
    if (outcome == Outcome::Back) {
        meterEditor_ = meterProperties();
        openPanel(Panel::Meter, meterEditor_, ui::MeterProperties::Appearance);
        return;
    }
    if (outcome == Outcome::Activate) { // A reset may drop the R/G/B rows: the action keeps its selection at its new index.
        const auto action = appearanceEditor_.at(navigator_.row());
        appearanceEditor_.reset(action);
        navigator_.reset(appearanceEditor_.index(action));
    }
    if (outcome == Outcome::Changed || outcome == Outcome::Activate) {
        auto &meter = prefs_.meter;
        const bool element = appearanceEditor_.at(navigator_.row()) == Editor::Element; // Selects, sets nothing.
        if (!element) {
            meter.palette = appearanceEditor_.palette(); meter.face = appearanceEditor_.face(); meter.appearance = appearanceEditor_.appearance();
            changed(now); dataRefresh_ = true;
        }
    }
    view_.properties(appearanceEditor_, navigator_);
}
// A choice in the DMM panel, taken at once: whatever differs from the instrument is applied; what
// is refused is said and the panel shows the instrument as it is.
void ApplicationController::meterChanged(uint32_t now) {
    const auto &meter=data_.meter();
    const auto refused = [&](const char *why) { view_.notify(why, now); meterEditor_ = meterProperties(); };
    if (meterEditor_.automatic() != (meter.latest().range==dmm::RangeMode::Auto) && !meterCommand(data_.setDmmAuto(meterEditor_.automatic())))
        return refused("Range command busy");
    if (meterEditor_.relative() != (meter.relative().active() || meter.latest().peerRelative) && !data_.relativeMeter(meterEditor_.relative()))
        return refused("REL needs a valid current reading");
    auto &logger=data_.meterLogger();
    if (meterEditor_.logging() != logger.active()) {
        if (logger.active()) logger.finish();
        else if (!logger.begin(1024*1024)) return refused("Cannot start CSV logging");
    }
    if (meterEditor_.serial() != data_.meterSerial().enabled()) {
        if (!data_.serialMeter(meterEditor_.serial())) return refused("Serial output busy; turn GEN off");
        serialWatched_=data_.meterSerial().enabled();
    }
    prefs_.meter.display=meterEditor_.display();
    prefs_.meter.statistics=meterEditor_.statistics();
    prefs_.meter.face=meterEditor_.face();
    prefs_.meter.palette=meterEditor_.palette();
    changed(now); dataRefresh_=true;
}
} // namespace openosc
