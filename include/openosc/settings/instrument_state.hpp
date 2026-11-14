#pragma once
#include "openosc/settings/preferences.hpp"
#include "openosc/acquisition/request.hpp"
#include "openosc/generator/waveform.hpp"

namespace openosc {
// One conversion contract for local commands, startup and SCPI.
class InstrumentState {
public:
    static ScopeRequest scope(const Preferences &preferences);
    static void capture(const ScopeRequest &request, Preferences &preferences);
    static void capture(const generator::Settings &settings, Preferences &preferences);
    static generator::Settings generator(const Preferences &preferences);
};
} // namespace openosc
