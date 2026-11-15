#pragma once
#include <cstdint>

namespace openosc {
class Filesystem;
// The guarded 360 MHz profile (docs/POWER.md, Q-03). The profile is outside the MCU's data sheet; a
// start on it that faults or stalls resets (docs/WATCHDOG.md), and the saved setting would select it
// again at every boot. The guard file on the volume (real persistence, the atomic JsonFiles writer;
// DTCM retention through the factory boot is not relied on) says whether the last start on it was
// confirmed:
// - arm(): "starting" is written (and verified) before the profile runs; without it 360 is not allowed;
// - confirm(): "ok" after BootConfirmation's stable time on the profile;
// - stop(): "stopped" on a deliberate exit before that (power off, restart, Recovery, a change to
//   200/280 or Remote): the next start tries 360 again. It does not claim stability.
// The guard file is read only: nothing is archived or renamed (the evidence of an unconfirmed start
// stays until the setting is durably 200).
// Only RunModes[OverclockedRunMode] is guarded; 200 and 280 are the factory profiles.
class BootGuard {
public:
    // Clear: both names missing, or the current file "ok"/"stopped". Pending: the current file
    // "starting", invalid or oversized, or only the previous name present (power lost between the
    // writer's renames), whatever it holds. Unreadable: no volume or a storage error.
    enum class State : uint8_t { Clear, Pending, Unreadable };
    // The settings' run mode, saved through the normal settings file.
    class Setting {
    public:
        virtual bool saveRunMode(uint32_t mode) = 0; // True only when settings.json was written.
    protected:
        ~Setting() = default;
    };
    struct Start {
        uint32_t mode;        // The profile to select now.
        bool armed;           // 360 was armed: BootConfirmation confirms it later.
        const char *notice;   // What the UI says once it starts (nullptr: nothing).
    };
    explicit BootGuard(Filesystem &fs) : fs_(fs) {}
    State read();
    bool arm();
    bool confirm();
    bool stop();
    // The boot's profile for the saved `requested` one.
    // - Clear: armed, then 360; an arm that fails gives 200 now, the setting kept (unavailable).
    // - Pending (the last 360 start was not confirmed): 200, the setting saved as 200 first, and only
    //   after that save the marker cleared; a failed save keeps it, so the next boot falls back again.
    // - Unreadable: 200 for this boot only; neither the setting nor the marker is written.
    Start start(uint32_t requested, Setting &setting);

private:
    bool write(const char *text);
    Filesystem &fs_;
};

// When a running guarded profile is confirmed: StableMs of the main loop on it, healthy (UI and the
// measurement backend started), then confirm() once; a failed write is retried every RetryMs while
// the profile still runs. Leaving the profile ends the wait (the marker stays: not confirmed).
class BootConfirmation {
public:
    static constexpr uint32_t StableMs = 15000, RetryMs = 30000;
    void armed(uint32_t now) { waiting_ = true; at_ = now + StableMs; }
    bool waiting() const { return waiting_; }
    // True when confirm() is due now.
    bool due(uint32_t now, bool guardedRuns, bool healthy) {
        if (!waiting_) return false;
        if (!guardedRuns) { waiting_ = false; return false; }
        return healthy && int32_t(now - at_) >= 0;
    }
    void written(bool ok, uint32_t now) { if (ok) waiting_ = false; else at_ = now + RetryMs; }
    void cancel() { waiting_ = false; } // A deliberate exit (BootGuard::stop) or the profile left.
    // A run-mode change from `previous` to `active`: true (and the wait ended) when it left an
    // unconfirmed guarded profile, so BootGuard::stop() is due. Leaving a confirmed one: nothing.
    bool left(uint32_t previous, uint32_t active, uint32_t guarded) {
        if (!waiting_ || previous != guarded || active == guarded) return false;
        waiting_ = false;
        return true;
    }

private:
    bool waiting_ = false;
    uint32_t at_ = 0;
};

// The instrument after a refused restart (ShutdownSequence::Steps::resume), in this order: the volume,
// the 360 MHz guard, the clock users, then USB. If the shutdown wrote "stopped" and 360 still runs, it
// is armed again; if that cannot be done (no volume, a failed write), the users, still stopped, move
// to 200 MHz first: 360 never runs without "starting". A 200 that does not lock leaves whatever
// ClockControl runs (HSI), reported, not hidden.
class RefusedRestart {
public:
    class Steps {
    public:
        virtual bool mount() = 0;           // The volume again; false: no volume (measurements still run).
        virtual bool guardedRuns() = 0;     // The 360 MHz profile is the one running.
        virtual bool arm() = 0;             // BootGuard::arm.
        virtual bool retimeSafe() = 0;      // The stopped users' clock to 200 MHz; true when it runs.
        virtual bool resumeUsers() = 0;     // DataAccess::resume.
        virtual void startUsb() = 0;        // On the clock that runs now.
    protected:
        ~Steps() = default;
    };
    enum class Guard : uint8_t { Untouched, Armed, Safe, SafeFailed };
    struct Result { bool resumed; Guard guard; };
    static Result resume(bool stopped, Steps &steps) {
        const bool mounted = steps.mount();
        Guard guard = Guard::Untouched;
        if (stopped && steps.guardedRuns())
            guard = mounted && steps.arm() ? Guard::Armed : steps.retimeSafe() ? Guard::Safe : Guard::SafeFailed;
        const bool resumed = steps.resumeUsers();
        steps.startUsb();
        return {resumed, guard};
    }
};
} // namespace openosc
