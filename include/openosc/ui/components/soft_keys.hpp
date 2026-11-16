#pragma once
#include <cstddef>
#include <cstdio>

namespace openosc::ui {
// What the four keys under the screen do when something other than the instrument's own screen is
// in front: a panel or menu of rows, a field being edited, a confirmation, a calibration, the image
// list or preview, the Remote lock. One description, built from the state the key handlers read
// (ApplicationController::refreshSoftKeys), one renderer (ParameterBar::context); the instrument
// screens keep their own cards underneath, restored when the context ends.
struct SoftKey {
    const char *title = "", *value = "";
    bool live = false; // The key acts here. An inert card is drawn muted and never accented.
};
struct SoftKeys {
    static constexpr unsigned Count = 4, None = Count;
    SoftKey keys[Count];
    unsigned selected = None; // The live card of the panel open now, accented (None: no accent).

    // Rows (PropertyNavigator): F4 goes back from the list, or cancels the field being edited.
    static SoftKeys list(bool editing) {
        SoftKeys k;
        k.keys[3] = editing ? SoftKey{"Cancel", "Field", true} : SoftKey{"Back", "", true};
        return k;
    }
    // A step F4 abandons (a confirmation, a calibration in progress).
    static SoftKeys cancel(const char *what) {
        SoftKeys k;
        k.keys[3] = {"Cancel", what, true};
        return k;
    }
    // The Remote lock: no local key acts (MENU hold leaves it).
    static SoftKeys locked() {
        SoftKeys k;
        k.keys[0] = {"Remote", "Locked", false}; // Short: a user's Mono card font keeps it whole.
        return k;
    }
    // The live keys in one line, where the cards cannot be shown (the image preview's 256x192 picture
    // leaves no room for them): "F2 Full  F3 Delete  F4 Back".
    void line(char *output, size_t capacity) const {
        if (!capacity) return;
        size_t used = 0;
        output[0] = 0;
        for (unsigned i = 0; i < Count && used < capacity; ++i) {
            if (!keys[i].live) continue;
            const int n = std::snprintf(output + used, capacity - used, "%sF%u %s", used ? "  " : "", i + 1, keys[i].title);
            if (n < 0) return;
            used += size_t(n);
        }
    }
};
} // namespace openosc::ui
