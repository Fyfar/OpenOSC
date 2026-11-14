#include "openosc/recovery/controller.hpp"

namespace openosc {
void RecoveryController::formatStorage() {
    if (!storageReady_ || !formatter_.begin()) {
        message("FORMAT FAILED",storageReady_ ? formatter_.error() : "STORAGE NOT READY"); return;
    }
    state_=State::Formatting;
    screen_.clear(); screen_.text(8,10,"FORMAT EXTERNAL DISK",Display::Cyan);
    screen_.progress(formatter_.phase(),0,3);
}
void RecoveryController::serviceFormat() {
    if (state_!=State::Formatting) return;
    formatter_.service();
    if (formatter_.state()==fat::Formatter::State::Failed) { message("FORMAT FAILED",formatter_.error()); return; }
    if (!formatter_.busy()) { message("FORMAT COMPLETE","NEW FAT READY. RETURN TO USB DISK."); return; }
    screen_.progress(formatter_.phase(),formatter_.completedSteps(),3);
}
} // namespace openosc
