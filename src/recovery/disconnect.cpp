#include "openosc/recovery/controller.hpp"
#include "openosc/platform/input.hpp"

namespace openosc {
void RecoveryController::disconnectKey(uint32_t key) {
    if ((key & Keypad::F4) || ((key & Keypad::Menu) && !yes_)) {
        showMain();
        return;
    }
    if (key & (Keypad::Left | Keypad::Right)) {
        yes_ = !yes_;
        views_.disconnect(yes_);
    } else if ((key & Keypad::Menu) && yes_) {
        views_.message("DISCONNECTING USB DISK", "PLEASE WAIT...");
        if (!usb_.stop()) {
            message("USB DISCONNECT FAILED", "FINISH COPYING FILES AND RETRY.");
            return;
        }
        begin(action_);
    }
}
}
