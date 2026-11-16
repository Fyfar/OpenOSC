#include "openosc/ui/settings/view.hpp"
#include "openosc/board/current.hpp"
#include <cstdio>

#ifndef OPENOSC_VERSION
#define OPENOSC_VERSION "development"
#endif
namespace openosc::ui {
void SettingsView::information(const SettingsMenu &menu, const DeviceInfo &info) {
    breadcrumb(menu);
    if (menu.page() == SettingsPage::About) {
        text("OpenOSC " OPENOSC_VERSION, 12, 54, 280, 14);
        text(board::Current::AboutText, 12, 82, 280, 14);
        text("LVGL 9.6.0", 12, 110);
        return;
    }
    char value[80];
    std::snprintf(value, sizeof value, "External NOR: %lu MiB", static_cast<unsigned long>(info.capacity / 1048576));
    text(value, 12, 54, 280, 14);
    std::snprintf(value, sizeof value, "JEDEC: %06lX", static_cast<unsigned long>(info.jedec));
    text(value, 12, 80);
    text(info.storageReady ? "FAT: Mounted" : "FAT: Unavailable", 12, 104);
    if (info.storageReady) {
        std::snprintf(value, sizeof value, "Free: %lu KiB", static_cast<unsigned long>(info.freeBytes / 1024));
        text(value, 12, 128); // Within the menu above the cards.
    }
}
} // namespace openosc::ui
