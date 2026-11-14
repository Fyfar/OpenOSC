#pragma once
#include <cstdint>

namespace openosc::usb {
// RM0455 section 6.4.8: VDD33USB is checked (PWR_CR3.USB33RDY, detector USB33DEN) before the
// transceiver is used. Bounded twice: by `timeoutMs` of `now`, and by `polls` reads, so that a
// stalled tick (interrupts masked during initialization) cannot hang it.
template <class Ready, class Now>
bool supplyReady(Ready ready, Now now, uint32_t timeoutMs, uint32_t polls) {
    const uint32_t start = now();
    for (uint32_t i = 0; i < polls; ++i) {
        if (ready()) return true;
        if (uint32_t(now() - start) >= timeoutMs) break;
    }
    return ready();
}
} // namespace openosc::usb
