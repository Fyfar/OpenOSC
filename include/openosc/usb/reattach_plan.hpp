#pragma once
#include <cstdint>

namespace openosc::usb {
// When the instrument USB device attaches again after a planned detach (a measurement-profile or
// core-clock change). The stop releases D+ (USBD_DeInit -> USBD_LL_Stop); the start is due
// DetachMs after the request and runs in the main loop, so the host sees a real detach and the UI
// and data settle first. A start that follows such a detach needs no blocking detach interval of
// its own; a start without one (boot: Factory/Recovery may leave a connected core) keeps it.
class ReattachPlan {
public:
    static constexpr uint32_t DetachMs = 300;
    void stopped(uint32_t now) { detached_ = true; detachedAt_ = now; }
    void cancel() { pending_ = false; } // Every stop: a later request alone attaches again.
    void request(uint32_t now) { pending_ = true; dueAt_ = now + DetachMs; }
    bool pending() const { return pending_; }
    // True once, when the requested start is due.
    bool due(uint32_t now) {
        if (!pending_ || int32_t(now - dueAt_) < 0) return false;
        pending_ = false;
        return true;
    }
    bool needsDetach(uint32_t now) const { return !detached_ || uint32_t(now - detachedAt_) < DetachMs; }
    void started() { detached_ = false; pending_ = false; } // Attached: nothing left to start.

private:
    uint32_t detachedAt_ = 0, dueAt_ = 0;
    bool detached_ = false, pending_ = false;
};
} // namespace openosc::usb
