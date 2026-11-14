#pragma once
#include <cstdint>

namespace openosc::usb {
// The instrument's USB data link (CDC, USBTMC) runs only while the core runs this profile (200 MHz).
// At 360 MHz Root measured a configured device the host reset every 5-8 s (usbResets 53 -> 56 in 6 s,
// address 35 -> 0 -> 36; build/hardware/openosc-files-review/usb-icon-360-passive.json). On every
// other profile, and on the HSI fallback, the device is stopped. Charging, VBUS sensing and the battery
// do not depend on it; Recovery's MSC is a separate device.
struct ClockPolicy {
    static constexpr uint32_t Mode = 0;
    static constexpr bool allows(uint32_t activeMode) { return activeMode == Mode; }
};
} // namespace openosc::usb
