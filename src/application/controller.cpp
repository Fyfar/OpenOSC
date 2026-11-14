#include "openosc/application/controller.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/platform/input.hpp"
#include "openosc/platform/input_service.hpp"
#include "openosc/platform/cooperative_service.hpp"
#include "openosc/ui/render_idle.hpp"
#include "openosc/ui/scope/column_cache.hpp"
#include "openosc/platform/power.hpp"
#include "openosc/platform/clocks.hpp"
#include "openosc/application/scpi_backend.hpp"
#include "openosc/application/power_hold.hpp"
#include "openosc/usb/instrument_usb.hpp"
#ifdef OPENOSC_DIAG_MAILBOX
#include "openosc/application/local_control.hpp"
#endif
#include <cstdio>

namespace openosc {
[[noreturn]] void ApplicationController::run() {
    prefs_ = preferences_.values();
    applyPreferences();
    battery_.initialize();
    data_.initialize(state_.mode);
    const auto applied = applyDataPreferences();
    InstrumentBackend tmcBackend(*this), serialBackend(*this);
    scpi::Engine tmcEngine(tmcBackend), serialEngine(serialBackend);
    InstrumentUsb usb(tmcEngine,serialEngine); usb_ = &usb;
#ifdef OPENOSC_DIAG_MAILBOX
    // Diagnostic builds: the SWD mailbox, served through a backend of its own (local_control.hpp).
    InstrumentBackend localBackend(*this);
    diag::LocalControl local(localBackend);
#endif
    uiReady_ = display_.initialize();
    // While a refresh waits on DMA2D: the watchdog, queued USB transfers and the acquisition
    // streams go on (no LVGL, FAT or UI state there; see RenderIdle).
    RenderIdle::install([](void *context, uint32_t now) {
        Board::service();
        CooperativeService::poll(now);
        auto &self = *static_cast<ApplicationController *>(context);
        self.data_.serviceBackground(now);
        self.prepareColumns();
    }, this);
    view_.waveform().columnCache(&plot::applicationColumns());
    boot_diagnostics.stage = 10;
    if (uiReady_) {
        if (!prefs_.useBuiltinTheme) themes_.reload();
        view_.build(themes_.theme()); view_.measurement(state_);
        applyBrightness();
        char result[112]; applied.text(result,sizeof result);
        if (!applied.ok()) view_.notify(result,Board::milliseconds());
        else if (preferences_.error()) view_.notify(preferences_.error(), Board::milliseconds());
        else if (preferences_.repaired() && preferences_.restored()) view_.notify("Bad settings archived; previous copy restored", Board::milliseconds());
        else if (preferences_.repaired()) view_.notify("Bad settings archived; defaults written", Board::milliseconds());
        else if (preferences_.restored()) view_.notify("Settings restored from previous copy", Board::milliseconds());
        else if (themes_.repaired()) view_.notify("Bad theme archived; defaults restored", Board::milliseconds());
        else if (themes_.error()) view_.notify(themes_.error(), Board::milliseconds());
        else if (data_.zeroFile().error()) view_.notify(data_.zeroFile().error(),Board::milliseconds());
        else if (data_.zeroFile().repaired()) view_.notify("Bad zero JSON preserved; profile restored",Board::milliseconds());
        else if (boot_diagnostics.error==4) view_.notify("High unavailable; Normal active",Board::milliseconds());
    } else { // Headless: no UI; Remote (and its 200 MHz) only if that profile really runs.
        boot_diagnostics.error = 2; display_.enabled(false); setRemote(true);
    }
    usb.followClock(ClockControl::activeMode()); // The boot profile: a high one leaves USB data off.
    if (usb.allowed() && !usb.start()) boot_diagnostics.error = 3;
    boot_diagnostics.stage = 11;
    performance_.start(DWT->CYCCNT);
    InputService::enable();
    while (true) {
        performance_.tick(DWT->CYCCNT);
        Board::service();
        const uint32_t now = Board::milliseconds();
        data_.poll(now);
        serviceShutdown(now);
        usb.poll(now);
#ifdef OPENOSC_DIAG_MAILBOX
        local.poll(now);
#endif
        serviceRecovery(now);
        if (uiReady_) view_.usbConnected(usb.connected());
        serviceAuto(now);
        serviceInstruments(now);
        serviceZeroUi(now);
        {
            // Full-rate capture restarts under a large redraw, or while a host enumerates, have
            // coincided with USB33RDY dips and bus resets on the device (cause not established).
            // A hidden trace needs no new frames, and such a record takes under a millisecond when
            // one is asked for, so those restarts wait.
            if (uiReady_ && !view_.waveformVisible() && !data_.zeroBusy() && !autoBusy() &&
                data_.scopeRequest().trigger.mode == scope::TriggerMode::Auto) hiddenAt_ = now;
            const bool settling = uint32_t(now - hiddenAt_) < OverlaySettleMs || usb.enumerating(now);
            data_.pauseCapture(state_.mode == MeasurementMode::Oscilloscope && settling && data_.fullRateCapture(), now);
        }
        serviceImages(now);
        if (uiReady_ && !shots_.busy()) {
            const uint32_t start = DWT->CYCCNT;
            displayData(now);
            performance_.ui(DWT->CYCCNT-start);
        }
        const bool activity=InputService::activity();
        const bool wake=servicePower(InputService::keys() || activity,now);
        if (wake) InputService::wake();
        if (InputService::overflowed()) {
            InputService::suppress();
            if (uiReady_) view_.notify("Input overflow; press again",now);
        }
        if (uiReady_) { // The REL/Power hold's progress, from the input sampler's own timing.
            uint32_t held = 0;
            const bool holding = shutdown_ == Shutdown::None && InputService::powerHold(held);
            view_.powerHold(PowerHold::percent(holding, held));
        }
        TimedKeyEvents input;
        if (shots_.busy()) {
            shots_.service();
            boot_diagnostics.progress = shots_.percent();
            if (!shots_.busy()) {
                if (imageShowing_) loadImage(now);
                view_.notify(shots_.succeeded() ? shots_.path() : shots_.error(), now);
            } else if (shots_.percent()/5 != shotProgress_/5) {
                shotProgress_=shots_.percent();
                char progress[40]; std::snprintf(progress,sizeof progress,"Saving screenshot: %u%%",shotProgress_);
                view_.notify(progress,now);
            }
            if (uiReady_) { view_.service(now); render(); }
        } else {
            if (InputService::take(input)) onKeys(input.events,now);
            store(now);
            if (uiReady_) {
                const uint32_t start = DWT->CYCCNT;
                view_.service(now);
                if (!shots_.busy()) render();
                performance_.ui(DWT->CYCCNT-start);
            }
        }
        boot_diagnostics.action = uint32_t(state_.mode) | (state_.settingsOpen ? 0x100 : 0)
            | (state_.settingsOpen && settingsMenu_.information() ? 0x200 : 0);
        // Keep the foreground polling loop awake while the PIO USB core is active.
        // WFI caused reproducible EP0 stalls/re-enumeration on this ZT703S.
    }
}
// One refresh; its waits on DMA2D reduce the next frame ahead (prepareColumns). The look at that
// frame ends with the refresh: no lease stays held between loops (clock changes, SCPI).
void ApplicationController::render() {
    rendering_ = true;
    display_.render();
    rendering_ = false;
    ahead_.release();
}
// While the display waits on DMA2D (RenderIdle, no LVGL): the newest frame not yet shown, looked
// at without being taken, has its columns reduced into the ColumnCache the projection then uses.
void ApplicationController::prepareColumns() {
    if (!rendering_ || state_.mode != MeasurementMode::Oscilloscope || panel_ != Panel::None || state_.settingsOpen || !uiReady_ ||
        data_.scopeHeld() || !view_.waveformVisible()) return;
    const auto &model = view_.waveform().model();
    if (!ahead_.valid()) {
        if (!data_.acquireScope(ahead_, false)) return;
    }
    if (model.hasFrame() && ahead_.view().metadata().sequence == model.metadata().sequence) {
        ahead_.release(); // Already shown: do not reduce it again or hold up publication of the next frame.
        return;
    }
    plot::applicationColumns().prepare(ahead_.view(), model.options(), model.viewport().width, model.viewport().height);
}
void ApplicationController::applyPreferences() {
    state_.mode = static_cast<MeasurementMode>(prefs_.mode);
    state_.scope.timebaseNs = prefs_.timebaseNs;
    state_.scope.measurements = prefs_.scope.measurements;
    for (unsigned c = 0; c < 2; ++c) state_.scope.millivoltsPerDivision[c] = prefs_.millivolts[c];
    state_.meter.function = prefs_.meterFunction;
    idle_.configure(prefs_.idle,Board::milliseconds());
    dimmed_=false;
    applyBrightness();
    dataRefresh_=true;
}
void ApplicationController::onKeys(const KeyEvents& events, uint32_t now) {
    const uint32_t keys = events.shortPress | events.repeat;
    if (shutdown_ != Shutdown::None || recoveryPending_) return;
    // REL/Power held to the end of its progress (PowerHold): the power-off pipeline at once, from
    // every screen, menu, dialog, Remote or headless; it saves settings and files and stops the
    // instruments, and says why when it cannot.
    if (events.longPress & Keypad::Relative) {
        if (panel_==Panel::Zero) data_.cancelZero(now);
        if (dropEdit(now) && uiReady_) showSettings(); // Not saved, not kept if the power off is refused.
        confirmAction_=ShutdownAction::PowerOff; requestShutdown(now);
        return;
    }
    if (panel_==Panel::Zero) { zeroKeys(keys,now); return; }
    if (remote_ || !uiReady_) { // Locked: Remote, or no UI to operate (headless).
        if ((events.longPress & Keypad::Menu) && setRemote(false)) {
            state_.settingsOpen = true; settingsMenu_.open(); navigator_.reset(); showSettings();
        }
        return;
    }
    if (panel_ == Panel::Confirmation) { confirmationKeys(keys,now); return; }
    if (panel_ == Panel::Images) {
        if (events.longPress&Keypad::Menu) { closeImages(); settingsMenu_.open(); navigator_.reset(); showSettings(); }
        else if ((events.longPress&Keypad::HoldSave) && !imageReader_.busy() && !images_.clearing()) {
            startScreenshot(now);
        } else imageKeys(keys,now);
        return;
    }
    if (events.shortPress & Keypad::Mode) {
        if (selectMode(nextMode(state_.mode), now) == ModeChange::Refused) view_.notify("Cannot switch measurement hardware",now);
        return;
    }
    if ((events.longPress & Keypad::Menu) && !state_.settingsOpen) {
        if (!cancelAuto(now)) return;
        panel_ = Panel::None;
        state_.settingsOpen = true;
        settingsMenu_.open();
        navigator_.reset();
        showSettings();
    } else if (events.longPress & Keypad::HoldSave) {
        startScreenshot(now);
    } else if ((events.longPress & Keypad::F3) && state_.mode == MeasurementMode::Oscilloscope &&
               !state_.settingsOpen && panel_ == Panel::None && !autoBusy() && !data_.zeroBusy()) {
        triggerMidpoint(now); // F3 hold, as on the factory firmware.
    } else if ((events.longPress & Keypad::F2) && state_.mode == MeasurementMode::Oscilloscope &&
               !state_.settingsOpen && panel_ == Panel::None && !autoBusy() && !data_.zeroBusy()) {
        if (centerChannels(now)) view_.notify("Channels centered", now); // F2 hold, as on the factory firmware.
    } else if ((events.longPress & Keypad::AutoRange) && state_.mode == MeasurementMode::Multimeter &&
               !state_.settingsOpen && panel_ == Panel::None) {
        meterRangeHold(now);
    } else if (state_.settingsOpen) settingsKeys(keys, now);
    else panelKeys(keys, now, !(events.repeat & (Keypad::Up|Keypad::Down|Keypad::Left|Keypad::Right)));
}
void ApplicationController::changed(uint32_t now) { savePending_ = true; saveAt_ = now + 1000; }
void ApplicationController::store(uint32_t now) {
    // Erasing the external Flash dips the supply shared with the USB PHY hard enough
    // to drop the host, so saves wait until no host is present (STORE-02).
    // Shutdown and the Recovery handoff still flush them.
    if (usb_ && usb_->hostActive(now)) return;
    if (themePending_ && int32_t(now - themeAt_) >= 0) {
        if (themes_.savePlot(view_.waveform().options())) themePending_ = false;
        else { themeAt_=now+5000; if (uiReady_) view_.notify(themes_.error(),now); }
    }
    if (!savePending_ || int32_t(now - saveAt_) < 0) return;
    usbtrace::mark(usbtrace::Tag::SaveBegin);
    const bool saved = preferences_.save(prefs_);
    usbtrace::mark(usbtrace::Tag::SaveEnd);
    if (saved) savePending_=false;
    else { saveAt_=now+5000; if (uiReady_) view_.notify(preferences_.error(),now); }
    if (uiReady_ && state_.settingsOpen && !settingsMenu_.information()) showSettings();
}
void ApplicationController::queuePlotSave(uint32_t now) {
    themePending_ = true;
    themeAt_ = now + 1000;
    prefs_.useBuiltinTheme = 0;
    changed(now);
}
void ApplicationController::showSettings() {
    if (settingsMenu_.information()) {
        view_.information(settingsMenu_, settingsInfo_);
        return;
    }
    char saved[48];
    std::snprintf(saved, sizeof saved, "Saved: %s", storage::Layout::SettingsFile);
    const char* status = savePending_ ? (usb_ && usb_->connected() ? "Saves when USB host disconnects" : "Saving settings...")
        : preferences_.error() ? preferences_.error() : saved;
    view_.settings(settingsMenu_, settingsRows(), navigator_, status);
}
}
