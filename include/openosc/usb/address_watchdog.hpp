#pragma once
#include <cstdint>

namespace openosc::usb {
// A short bus glitch can reset the USB core while the host keeps using the old
// address; the host then never enumerates again. This decides when to force a
// re-enumeration: a bus reset that no SETUP follows within QuietMs.
class AddressWatchdog {
public:
    static constexpr uint32_t QuietMs = 1000;
    static constexpr unsigned MaxAttempts = 3;
    static constexpr uint32_t ReattachMs = 8000;

    // Returns true when the device should disconnect and reconnect now.
    // A configured device with an assigned address has finished enumerating, so a
    // reset is over even when no further SETUP follows on an idle bulk interface.
    bool update(uint32_t now, uint32_t resets, uint32_t setups, bool configured = false) {
        if (configured) {
            primed_ = true;
            resets_ = resets;
            setups_ = setups;
            waiting_ = false;
            attempts_ = 0;
            reattachAt_ = now + ReattachMs;
            return false;
        }
        if (!primed_) {
            primed_ = true;
            resets_ = resets;
            setups_ = setups;
            reattachAt_ = now + ReattachMs;
            return false;
        }
        if (resets != resets_) {
            resets_ = resets;
            setups_ = setups;
            resetAt_ = now;
            waiting_ = true;
            hostSeen_ = true;
            reattachAt_ = now + ReattachMs;
            return false;
        }
        if (setups != setups_) {
            setups_ = setups;
            waiting_ = false;
            attempts_ = 0;
            reattachAt_ = now + ReattachMs;
            return false;
        }
        if (waiting_ && int32_t(now - resetAt_) >= int32_t(QuietMs)) {
            waiting_ = false;
            if (attempts_ < MaxAttempts) {
                ++attempts_;
                reattachAt_ = now + ReattachMs;
                return true;
            }
        }
        // A host that gave up enumerating ignores the port until the device reattaches.
        if (hostSeen_ && int32_t(now - reattachAt_) >= 0) {
            reattachAt_ = now + ReattachMs;
            return true;
        }
        return false;
    }

private:
    uint32_t resets_ = 0, setups_ = 0, resetAt_ = 0, reattachAt_ = 0;
    unsigned attempts_ = 0;
    bool primed_ = false, waiting_ = false, hostSeen_ = false;
};
} // namespace openosc::usb
