#include "openosc/application/controller.hpp"
#include "openosc/platform/clocks.hpp"
#include "openosc/platform/power.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/usb/instrument_usb.hpp"

namespace openosc {
namespace {
// The firmware's steps of a run-mode change: the data owners, PLL1, then the platform timers
// that are not data owners (backlight PWM, the USB trace sampler in trace builds).
class ClockSteps final : public RunModeSwitch::Steps {
public:
    ClockSteps(DataAccess &data, ReadoutScheduler &readouts) : data_(data), readouts_(readouts) {}
    ClockSuspend suspend() override {
        readouts_.suspend(data_); // A background readout holds a frame lease; give it back first.
        return data_.suspendClockUsers();
    }
    bool retime(uint32_t mode) override {
        const bool locked = ClockControl::selectPerformance(mode);
        PowerControl::retime(); // From whatever runs now, also after a failed attempt.
        usbtrace::start();
        return locked && ClockControl::activeMode() == mode;
    }
    bool resume() override { return data_.resumeClockUsers(); }
    uint32_t active() const override { return ClockControl::activeMode(); }
private:
    DataAccess &data_;
    ReadoutScheduler &readouts_;
};
// The instrument USB device across a run-mode change: de-initialized (D+ released) before PLL1
// and the AHB change; after it follows the clock that runs (InstrumentUsb::followClock) and, where
// that is 200 MHz, is initialized again by InstrumentUsb::poll after the settle interval. From
// SYST:CLOC:MODE this ends the session that sent it; the host opens a new one.
class UsbLink final : public RunModeSwitch::Link {
public:
    explicit UsbLink(InstrumentUsb *usb) : usb_(usb) {}
    bool attached() const override { return usb_ && usb_->attached(); }
    void detach() override { usb_->stop(); }
    void settle(uint32_t active) override {
        if (!usb_) return;
        usb_->followClock(active);
        usb_->reattach(Board::milliseconds()); // Nothing where the clock does not allow it.
    }
private:
    InstrumentUsb *usb_;
};
} // namespace
RunModeSwitch::Result ApplicationController::changeRunMode(uint32_t mode, uint32_t now, bool persist) {
    // Operations that time themselves or hold a file across polls finish first.
    if (autoBusy() || shots_.busy() || imageReader_.busy() || recoveryPending_ || shutdown_ != Shutdown::None)
        return {RunModeSwitch::Outcome::Refused, ClockControl::activeMode()};
    ClockSteps steps(data_, readouts_);
    UsbLink link(usb_);
    const auto result = RunModeSwitch::change(steps, mode, link);
    const bool runs = result.outcome == RunModeSwitch::Outcome::Applied || result.outcome == RunModeSwitch::Outcome::Unchanged ||
                      result.outcome == RunModeSwitch::Outcome::ResumeFailed;
    if (persist && runs && result.active == mode && prefs_.scope.runMode != mode) {
        prefs_.scope.runMode = mode; // Saved to settings.json by the usual delayed save, no restart.
        changed(now);
    }
    readouts_.reset(data_); // Measurement caches and their timings start again.
    dataRefresh_ = true;
    return result;
}
} // namespace openosc
