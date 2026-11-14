#include "openosc/recovery/fat_check.hpp"
#include "openosc/platform/board.hpp"

namespace openosc {
void FatCheckOperation::progress(const char* phase, uint32_t done, uint32_t total) {
    Board::service();
    if (done != total && Board::milliseconds() - lastDraw_ < 80) return;
    lastDraw_ = Board::milliseconds();
    screen_.progress(phase, done, total);
}
bool FatCheckOperation::inspect() {
    screen_.clear();
    screen_.text(8, 10, "CHECK FAT (READ ONLY)", Display::Cyan);
    bool success = checker_.check();
    fingerprint_ = checker_.report().fingerprint;
    return success;
}
bool FatCheckOperation::repair() {
    screen_.clear();
    screen_.text(8, 10, "REPAIR FAT", Display::Cyan);
    return checker_.repair(fingerprint_);
}
}
