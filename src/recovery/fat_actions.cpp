#include "openosc/recovery/controller.hpp"
#include "openosc/platform/input.hpp"

namespace openosc {
void RecoveryController::checkFat() {
    if (!storageReady_ || !fs_.unmount()) { message("FAT CHECK FAILED", "STORAGE NOT READY"); return; }
    if (!fatCheck_.inspect()) { message("FAT CHECK FAILED", fatCheck_.error()); return; }
    state_ = State::FatReport;
    views_.fatReport(fatCheck_.report());
}
void RecoveryController::repairFat() {
    if (!fatCheck_.repair()) { message("FAT REPAIR FAILED", fatCheck_.error()); return; }
    state_ = State::FatReport;
    views_.fatReport(fatCheck_.report());
}
void RecoveryController::fatReportKey(uint32_t key) {
    if (key & Keypad::F4) { showMain(); return; }
    if (!(key & Keypad::Menu)) return;
    const auto& report = fatCheck_.report();
    if (report.blocked() || !report.fixes()) { showMain(); return; }
    state_ = State::Confirm;
    yes_ = false;
    drawConfirm();
}
}
