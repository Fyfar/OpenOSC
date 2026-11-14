#pragma once
#include "openosc/measurement/dmm/ranges.hpp"
#include "openosc/settings/instrument_preferences.hpp"

namespace openosc {
// The DMM range the settings remember: Auto, or a manual range together with the function it was
// confirmed in (MeterPreferences::rangeFunction). Pure decisions; the controller sends the commands.
struct MeterRangeMemory {
    // On entering the DMM, once the controller shows `function` (`reported` its range state): restore
    // Auto, or the manual range only in its own function. An unknown owner (older files), another
    // function or a function without ranges: nothing (the controller's state is then taken).
    static bool restores(const MeterPreferences &saved, uint32_t savedFunction, dmm::Function function, dmm::RangeMode reported) {
        if (unsigned(function) != savedFunction || reported == dmm::RangeMode::Unknown) return false;
        return saved.automatic || saved.rangeFunction == unsigned(function);
    }
    // A confirmed reading into the settings (only after a command or selection that succeeded: the
    // caller checks). True when they changed.
    static bool mirror(MeterPreferences &saved, dmm::Function function, const dmm::Result &result) {
        if (result.range == dmm::RangeMode::Auto) {
            if (saved.automatic) return false;
            saved.automatic = 1; return true;
        }
        unsigned index = 0;
        if (result.range != dmm::RangeMode::Manual || !dmm::Ranges::current(result, &index)) return false;
        if (!saved.automatic && saved.range == index && saved.rangeFunction == unsigned(function)) return false;
        saved.automatic = 0; saved.range = index; saved.rangeFunction = unsigned(function); return true;
    }
};
} // namespace openosc
