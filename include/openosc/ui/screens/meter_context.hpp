#pragma once
#include "openosc/measurement/dmm/session.hpp"
#include "openosc/settings/meter_appearance.hpp"

namespace openosc::ui {
struct MeterContext {
    const dmm::Statistics *statistics = nullptr;
    const dmm::History *history = nullptr;
    const dmm::RelativeReading *relative = nullptr;
    unsigned display = 0;
    bool showStatistics = true;
    unsigned face = 0, palette = 0; // MeterStyles: the reading's typeface and the DMM colours.
    const MeterAppearance *appearance = nullptr; // Per-element overrides (null: none).
};
} // namespace openosc::ui
