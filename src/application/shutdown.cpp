#include "openosc/application/controller.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/input.hpp"
#include "openosc/platform/power.hpp"
#include "openosc/platform/recovery_request.hpp"
#include "openosc/ui/editors/confirmation.hpp"
#include "openosc/usb/instrument_usb.hpp"

namespace openosc {
void ApplicationController::confirmShutdown(ShutdownAction action) {
    confirmAction_=action;
    confirmOperation(ui::Confirmation(action),&ApplicationController::requestShutdown);
}
void ApplicationController::confirmOperation(const ui::Confirmation &question,ConfirmHandler handler) {
    confirmationImages_=panel_==Panel::Images;
    confirmationZero_=panel_==Panel::Zero;
    confirmationSettings_=state_.settingsOpen;
    cancelAuto(Board::milliseconds()); confirmation_=question; confirmationHandler_=handler;
    state_.settingsOpen = false;
    openPanel(Panel::Confirmation, confirmation_); // On Cancel.
}
void ApplicationController::confirmationKeys(uint32_t keys, uint32_t now) {
    if (!keys) return;
    using Outcome = ui::PropertyNavigator::Outcome;
    const auto outcome = (keys & Keypad::Mode) ? Outcome::Back : navigator_.keys(keys, confirmation_);
    if (outcome == Outcome::Back || (outcome == Outcome::Activate && navigator_.row() == 0)) {
        confirmationHandler_=nullptr;
        if (confirmationImages_) { panel_=Panel::Images; imageShowing_=false; showImages(); }
        else if (confirmationZero_ && data_.zeroBusy()) { panel_=Panel::Zero; zeroProgress_=UINT32_MAX; serviceZeroUi(now); }
        else if (confirmationSettings_) { panel_=Panel::None; state_.settingsOpen=true; navigator_.reset(settingsMenu_.selected()); showSettings(); }
        else closePanel();
        return;
    }
    if (outcome == Outcome::Activate && confirmationHandler_) { (this->*confirmationHandler_)(now); return; }
    view_.properties(confirmation_, navigator_);
}
// Headless (no LVGL: the REL/Power hold works there too) nothing is drawn; a refused or cancelled
// shutdown then shows only in its result (still running, USB back).
void ApplicationController::shutdownNotice(const char *text, uint32_t now) {
    if (uiReady_) view_.notify(text, now);
}
void ApplicationController::requestShutdown(uint32_t now) {
    if (shots_.busy()) { shutdownNotice("Finish screenshot first",now); return; }
    imageReader_.cancel(); images_.cancelClear();
    if (themePending_) {
        if (themes_.savePlot(view_.waveform().options())) themePending_=false;
        else if (confirmAction_!=ShutdownAction::Recovery) { shutdownNotice(themes_.error(),now); return; }
    }
    if (savePending_) {
        if (preferences_.save(prefs_)) savePending_=false;
        else if (confirmAction_!=ShutdownAction::Recovery) { shutdownNotice(preferences_.error(),now); return; }
    }
    if (usb_) usb_->stop();
    data_.beginShutdown(); shutdown_ = confirmAction_;
    shutdownAt_ = now + 10000; shutdownNotice("Finishing files...",now);
}
void ApplicationController::serviceShutdown(uint32_t now) {
    if (shutdown_ == Shutdown::None) return;
    bool failed = false;
    if (!data_.shutdownReady()) {
        if (int32_t(now - shutdownAt_) < 0) return;
        data_.scopeRecorder().cancel(); data_.meterLogger().cancel(); failed = true;
    }
    if (!failed && fs_.unmount()) {
        if (shutdown_ == Shutdown::PowerOff) PowerControl::powerOff();
        if (nor_.restoreLegacyAddressing()) {
            if (shutdown_==Shutdown::Restart) NVIC_SystemReset();
            else RecoveryRequest::restart();
        }
        fs_.mountPrepared();
    }
    shutdown_ = Shutdown::None; panel_ = Panel::None;
    const bool resumed = data_.resume(); if (usb_) usb_->start();
    if (uiReady_) view_.measurement(state_);
    dataRefresh_ = true;
    shutdownNotice(resumed ? "Cannot finish storage; shutdown cancelled" : "Hardware stop failed; restart required",now);
}
} // namespace openosc
