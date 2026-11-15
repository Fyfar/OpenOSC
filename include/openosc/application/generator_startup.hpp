#pragma once
#include "openosc/settings/preferences.hpp"

namespace openosc {
// Startup output (Settings / Device / Power, OUTP:STAR, generator.startup_output): Off by default -
// the output is off after power-up; Restore last turns it on again at boot when the user's last ON or
// OFF was confirmed ON (the factory main restarts its saved output with the Scope: 703S 0804F498,
// 706S 08036CDC). Only a user's request (the key, OUTP, the boot's restore) changes what is kept, and
// only once the driver confirms that very state: the ZT703S output running (not after a failed halt)
// or halted, the ZT706S module's acknowledged ON or OFF. Stops the instrument makes itself (zero
// calibration, a run-mode change, shutdown) request nothing and change nothing.
class GeneratorStartup {
public:
    enum : int { Unknown = -1, Off = 0, On = 1 };
    template <class Output> static int confirmed(const Output &output) {
        return output.onConfirmed() ? On : output.offConfirmed() ? Off : Unknown;
    }
    void requested(bool on) { intent_ = on ? On : Off; } // A user's ON or OFF, before it is applied.
    void refused() { intent_ = Unknown; }                // The request was not taken: nothing to keep.
    // The requested state, once confirmed, into the preferences; true when it changed (save it when
    // Restore last is chosen). Until then (a frame on its way, a failed halt) nothing changes.
    bool follow(Preferences &prefs, int state) {
        if (intent_ == Unknown || state != intent_) return false;
        intent_ = Unknown;
        if (prefs.generatorLastOutput == unsigned(state)) return false;
        prefs.generatorLastOutput = unsigned(state);
        return true;
    }
    // Whether boot turns the output on: chosen, last confirmed on, and the saved settings applied
    // (never with settings that were refused).
    static bool restores(const Preferences &prefs, bool configured) {
        return prefs.generatorStartup && prefs.generatorLastOutput && configured;
    }

private:
    int intent_ = Unknown;
};
} // namespace openosc
