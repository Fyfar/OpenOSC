#include "openosc/recovery/screens.hpp"

namespace openosc {
void RecoveryScreens::fatReport(const fat::Report& r) {
    frame("FAT CHECK REPORT", r.blocked() || !r.fixes() ? "MENU=RETURN  F4=BACK" : "MENU=REPAIR OPTIONS  F4=BACK");
    screen_.text(8, 36, "FAT:"); screen_.number(38, 36, r.bits);
    screen_.text(84, 36, r.blocked() ? "REPAIR ON PC REQUIRED" : r.fixes() ? "REPAIR AVAILABLE" : "CLEAN",
                 r.blocked() ? Display::Red : r.fixes() ? Display::Cyan : Display::Green);
    const char* labels[] = {"FILES / DIRECTORIES:", "ERASED DIRECTORY ENTRIES:", "UNLINKED CLUSTERS:",
                           "CHAIN / ENTRY ERRORS:", "FAT COPY MISMATCHES:", "METADATA FIXES:", "FREE CLUSTERS:"};
    const uint32_t values[] = {r.files, r.erasedEntries, r.lostClusters, r.chainErrors + r.invalidEntries,
                              r.mirrorErrors, r.directorySizes + r.dirty + r.label + r.info, r.freeClusters};
    for (unsigned i = 0; i < sizeof values / sizeof *values; ++i) {
        screen_.text(8, 58 + i * 18, labels[i]); screen_.number(182, 58 + i * 18, values[i]);
    }
    screen_.text(222, 58, "/"); screen_.number(236, 58, r.directories);
    screen_.text(8, 196, "CHECK DOES NOT WRITE TO THE DISK.");
    screen_.present();
}
void RecoveryScreens::fatRepairConfirm(const fat::Report& r, bool yes) {
    confirm("REPAIR FAT?", "FILESYSTEM METADATA WILL BE CHANGED.", yes);
    screen_.text(8, 64, "UNLINKED CLUSTERS:"); screen_.number(122, 64, r.lostClusters);
    screen_.text(8, 84, "UNLINKED DATA WILL BE FREED.", Display::Red);
    screen_.text(8, 104, "KEEP POWER ON UNTIL COMPLETE.");
    screen_.present();
}
void RecoveryScreens::fatFormatConfirm(bool yes) {
    confirm("FORMAT EXTERNAL DISK?","ALL FILES AND USER ZERO WILL BE LOST",yes);
    screen_.text(8,64,"FACTORY / RECOVERY / APPLICATION ARE KEPT.");
    screen_.text(8,84,"KEEP POWER ON UNTIL COMPLETE.",Display::Red);
    screen_.present();
}
}
