#pragma once
#include "openosc/settings/preferences.hpp"
#include "openosc/ui/scope/controls.hpp"
#include "openosc/ui/screens/generator_screen.hpp"
#include "openosc/ui/screens/meter_screen.hpp"
#include "openosc/ui/screens/scope_screen.hpp"
#include "openosc/ui/editors/frequency_editor.hpp"
#include "openosc/ui/editors/property_list.hpp"
#include "openosc/ui/settings/view.hpp"
#include "openosc/ui/components/choice_popup.hpp"
#include "openosc/ui/components/battery_indicator.hpp"
#include "openosc/ui/components/record_overview.hpp"
#include "openosc/ui/images/browser.hpp"
#include "openosc/ui/components/menu_content.hpp"
#include "openosc/ui/components/power_hold_overlay.hpp"

namespace openosc {
class ApplicationView {
public:
    ApplicationView() : scope_(host_), meter_(host_), generator_(host_) {}
    void build(const Theme &theme);
    void measurement(const ApplicationState &state);
    void capture(const scope::FrameView &frame, bool held, unsigned channel, DataStatus health);
    void captureStatus(DataStatus status, bool held) { scope_.status(status, held); refreshIndicators(); }
    void scopeTrigger(const scope::TriggerSettings &trigger, const scope::ChannelMetadata &channel);
    void scopeCounters(const scope::CounterSnapshot &snapshot) { scope_.counters(snapshot); }
    bool scopeReadouts(const scope::ReadoutResult &result, const ui::ReadoutState &state) { return scope_.readouts(result, state); }
    bool scopeReadoutsPainted() const { return scope_.readoutsPainted(); }
    void scopeFooterPage() { scope_.nextFooterPage(); }
    const ui::ScopeFooter &scopeFooter() const { return host_.scopeFooter(); } // Its pages and basis indicator.
    void scopeAutoSetup(bool active) { scope_.autoSetup(active); refreshIndicators(); }
    void scopeCalibrating(bool active) { scope_.calibrating(active); refreshIndicators(); }
    void scopeTriggerState(scope::TriggerMode mode, bool waiting) { scope_.triggerState(mode, waiting); refreshIndicators(); }
    bool waveformVisible() const { return !host_.overlayActive(); }
    void scopeSettings(const scope::FrameMetadata &metadata, unsigned channel) { scope_.settings(metadata, channel); }
    void scopeContext(unsigned channel, plot::Focus focus) { scope_.context(channel, focus); }
    void scopeGenerator(const generator::Snapshot &output) { scope_.generator(output); }
    void focusParameter(unsigned index) { host_.parameters().select(index); }
    void multimeter(const dmm::Result &result, bool held, DataStatus status,
                    const dmm::Statistics *statistics = nullptr) { meter_.update(result, held, status, {statistics}); }
    void multimeter(const dmm::Result &result, bool held, DataStatus status, const ui::MeterContext &context) {
        meter_.update(result, held, status, context);
    }
    void generator(const generator::Snapshot &snapshot) { generator_.update(snapshot); }
    void multimeterStatus(const dmm::Result &result, bool held, DataStatus status, bool relative = false) { meter_.status(result, held, status, relative); }
    // Every 8 ms or so while the DMM shows: the bar, and the analog needle's motion.
    void multimeterBar(const dmm::Result &result, DataStatus status, const dmm::RelativeReading *relative, uint32_t now = lv_tick_get()) {
        meter_.fastBar(result,status,relative,now);
    }
    void generatorStatus(const generator::Snapshot &snapshot) { generator_.status(snapshot); }
    void generatorActive(bool enabled);
    void usbConnected(bool connected);
    void battery(const power::BatteryState &state) { battery_.update(state); }
    void batteryDisplay(uint32_t mode);
    void fps(bool visible, uint32_t value);
    WaveformWidget &waveform() { return host_.waveform(); }
    const ui::MeterSummary &meterSummary() { return host_.meterSummary(); }
    const ui::FunctionIcon &functionIcon() { return host_.functionIcon(); }
    const ui::MeterCards &meterCards() { return host_.meterCards(); }
    const ui::AnalogDial &dial() { return host_.dial(); }
    const ui::MeterLook &meterLook() const { return meter_.look(); } // The DMM look in force (tests).
    const ui::RecordOverview &recordOverview() const { return overview_; }
    lv_obj_t *element(ElementId id) { return host_.elements().get(id); }
    void properties(const ui::PropertyList &properties, unsigned selected); // Browsing on `selected`.
    void properties(const ui::PropertyList &properties, const ui::PropertyNavigator &navigator);
    void frequencyEditor(const ui::FrequencyEditor &editor);
    void settings(const ui::SettingsMenu &menu, const ui::SettingsProperties &rows, const ui::PropertyNavigator &navigator,
                  const char *persistence);
    void information(const ui::SettingsMenu &menu, const DeviceInfo &info);
    void images(const ui::ImageList &images);
    void imagePreview(const char *name,const uint16_t *pixels, bool fullSize = false);
    void notify(const char *text, uint32_t now);
    void powerHold(int percent) { powerHold_.show(percent); } // PowerHold::percent; -1 hides.
    void service(uint32_t now);

private:
    static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y) {
        return ui::Elements::label(parent, text, x, y);
    }
    static const lv_font_t *font(unsigned size) { return ui::Elements::font(size); }
    void menuFrame(const char *title, bool instrument = false, bool retained = false);
    void choice(const ui::PropertyList &list, const ui::PropertyNavigator &navigator, int top); // The open list, if any.
    ui::ChoicePopup choicePopup_;
    ui::GeneratorPreview draftPreview_;
    lv_obj_t *draftPreviewObject_ = nullptr;
    ui::MenuContent menuContent_;
    bool menuRetained_ = false;
    const Theme *theme_ = nullptr;
    ui::ScreenHost host_;
    ui::BatteryIndicator battery_;
    ui::ImageBrowserView imageBrowser_;
    ui::ScopeScreen scope_;
    ui::MeterScreen meter_;
    ui::GeneratorScreen generator_;
    lv_obj_t *menu_ = nullptr, *toast_ = nullptr, *generatorIndicator_ = nullptr, *usbIndicator_ = nullptr, *fps_ = nullptr, *trigger_ = nullptr, *edge_ = nullptr;
    ui::RecordOverview overview_;
    ui::PowerHoldOverlay powerHold_;
    void refreshIndicators();
    void placeFps();
    int statusRight_ = -1; // The pill edge the indicators were last laid out for.
    int triggerX_ = 0;     // The trigger place in use (StatusBarLayout::TriggerNear or TriggerFar).
    uint32_t fpsValue_ = 0xffffffffU;
    bool fpsWanted_ = false, fpsVisible_ = false, triggerShown_ = false;
    uint32_t toastUntil_ = 0;
    bool generatorOn_ = false, toastActive_ = false, usbConnected_ = false;
    MeasurementMode mode_ = MeasurementMode::Oscilloscope;
    uint32_t menuAccent_ = 0x38bdf8;
};
} // namespace openosc
