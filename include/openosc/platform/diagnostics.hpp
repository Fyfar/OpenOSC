#pragma once
#include <cstdint>
namespace openosc {
struct Diagnostics {
    uint32_t magic, stage, heartbeat, error;
    uint32_t jedecId, norBytes, keys, usbState, usbReads, usbWrites;
    uint32_t action, progress, flashStatus, faultPc, faultLr, faultCfsr;
    uint32_t factoryCrc;
    // Appended (tools reading the first 17 words are unaffected); docs/WATCHDOG.md.
    uint32_t watchdog;    // Watchdog::Status bits of this run.
    uint32_t resetFlags;  // RCC_RSR at this image's start, before the application clears it.
    // The previous run's ResetRecord, when one validated (else zero).
    uint32_t lastCause, lastSource, lastPc, lastLr, lastCfsr, lastHfsr, lastDetail;
};
extern "C" volatile Diagnostics boot_diagnostics;

}
