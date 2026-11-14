#pragma once
#include "openosc/ui/editors/property_list.hpp"

namespace openosc::ui {
// The keys of every property list, one way (Settings, Scope, DMM and GEN panels):
// Browse: Up/Down rows; MENU opens the row (a Choice list, a Number edit), switches a Toggle,
//         runs an Action or opens a Page; Right opens only a Page; F4 and Left go back.
// Choice: Up/Down move the draft in the list; MENU takes it; F4 and Left cancel.
// Number: Left/Right and Up/Down step the value; MENU keeps it; F4 restores it.
// One field at a time is committed: a Choice taken, a Toggle switched, a Number finished with MENU
// (Changed). A Number's steps (Stepped) and its F4 restore (Restored) are the edit in progress: an
// owner that previews live (brightness) applies them, every other owner only redraws its draft.
// The navigator changes the list only through choose(), adjust() and cancelEdit().
class PropertyNavigator {
public:
    enum class Mode : uint8_t { Browse, Choice, Number };
    enum class Outcome : uint8_t {
        None,     // Nothing to do (or only a redraw: the row or the draft moved).
        Changed,  // A field committed: a choice taken, a toggle, a number edit finished with a new value.
        Stepped,  // A number edit's step: the draft value only.
        Restored, // A number edit cancelled (F4): the value it began with is back in the list.
        Refused,  // The list refused the choice or toggle: its notice() says why.
        Back,     // F4 or Left in Browse.
        Activate, // MENU on an Action row.
        Open      // MENU or Right on a Page row.
    };
    void reset(unsigned row = 0) { row_ = row; mode_ = Mode::Browse; draft_ = 0; saved_ = 0; }
    Outcome keys(uint32_t keys, PropertyList &list);
    // Leaves a list or number edit as if F4 were pressed (MODE, Remote, power off while a list is
    // open); true if a number was restored, which its owner then applies.
    bool cancel(PropertyList &list);
    unsigned row() const { return row_; }
    Mode mode() const { return mode_; }
    unsigned draft() const { return draft_; }

private:
    void restore(PropertyList &list) const;
    unsigned row_ = 0, draft_ = 0;
    int32_t saved_ = 0;
    Mode mode_ = Mode::Browse;
};
} // namespace openosc::ui
