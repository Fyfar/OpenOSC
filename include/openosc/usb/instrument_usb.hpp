#pragma once
#include "openosc/scpi/tmc.hpp"
#include "openosc/usb/address_watchdog.hpp"
#include "openosc/usb/clock_policy.hpp"
#include "openosc/usb/reattach_plan.hpp"
#include "openosc/usb/tmc_pipe.hpp"
#include "usbd_core.h"

namespace openosc {
class InstrumentUsb {
public:
    InstrumentUsb(scpi::Engine &tmc, scpi::Engine &serial) : tmcEngine_(tmc), serialEngine_(serial), tmc_(tmc) {}
    InstrumentUsb(const InstrumentUsb &) = delete;
    InstrumentUsb &operator=(const InstrumentUsb &) = delete;
    // The data link follows the clock that runs (ClockPolicy): on a profile it does not allow the
    // device is stopped (D+ released) and start()/reattach() refuse, so neither a planned attach
    // nor the address watchdog brings it back until it is allowed again.
    void followClock(uint32_t activeMode) {
        allowed_ = usb::ClockPolicy::allows(activeMode);
        if (!allowed_) stop();
    }
    bool allowed() const { return allowed_; }
    bool start(); // False also when not allowed.
    // Full de-initialization (D+ released). Also from a command that poll() dispatches: the
    // class de-initialization advances the session generation, so the rest of that pass neither
    // feeds, dispatches nor transmits for it (usb::current), and synchronize() resets both
    // engines again before the next session's data.
    void stop();
    void poll(uint32_t now);
    // Attached again by poll(), through start() (full initialization, RCC reset of the OTG core),
    // ReattachPlan::DetachMs after `now`.
    void reattach(uint32_t now) {
        if (!allowed_) return;
        reattach_.request(now); busActivityAt_ = now;
    }
    void profileChanged(uint32_t now) { stop(); reattach(now); }
    bool attached() const { return active_ || reattach_.pending(); } // Or an attach pending.
    bool connected() const;
    // True after attaching and while a host keeps resetting the bus without finishing enumeration.
    bool enumerating(uint32_t now) const {
        return active_ && !connected() && (uint32_t(now-attachedAt_) < EnumerationMs || uint32_t(now-busActivityAt_) < HostQuietMs);
    }
    // True while a host is enumerating or using the device: power-hungry bursts wait.
    bool hostActive(uint32_t now) const;
private:
    static constexpr uint32_t HostQuietMs = 5000;
    static constexpr uint32_t EnumerationMs = 3000;
    uint32_t attachedAt_ = 0;
    uint32_t busActivityAt_ = 0, busResets_ = 0;
    struct Polling {
        explicit Polling(bool &flag):flag_(flag) { flag_=true; }
        ~Polling() { flag_=false; }
        bool &flag_;
    };
    void background(uint32_t now);
    void synchronize();
    void transfer(uint32_t now); // The CDC byte stream.
    void serviceTmc(uint32_t now, bool dispatch);
    void releaseOutNak();
    scpi::Engine &tmcEngine_, &serialEngine_;
    scpi::Tmc tmc_;
    usb::TmcPipe tmcPipe_{tmc_, tmcEngine_, usb::tmcRecovery};
    usb::AddressWatchdog watchdog_;
    usb::ReattachPlan reattach_;
    uint32_t generation_ = 0;
    bool active_ = false, polling_ = false, allowed_ = false;
    bool serialZlp_ = false;
};
} // namespace openosc
