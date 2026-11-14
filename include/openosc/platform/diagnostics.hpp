#pragma once
#include <cstdint>
namespace openosc {
struct Diagnostics {
    uint32_t magic, stage, heartbeat, error;
    uint32_t jedecId, norBytes, keys, usbState, usbReads, usbWrites;
    uint32_t action, progress, flashStatus, faultPc, faultLr, faultCfsr;
    uint32_t factoryCrc;
};
extern "C" volatile Diagnostics boot_diagnostics;

}
