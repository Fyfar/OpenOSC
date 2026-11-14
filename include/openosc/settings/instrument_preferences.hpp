#pragma once
#include "openosc/measurement/meter_function.hpp"
#include <cstdint>
#include "openosc/acquisition/readouts.hpp"
#include "openosc/acquisition/record_depths.hpp"
#include "openosc/settings/meter_appearance.hpp"

namespace openosc {
struct ScopePreferences {
    uint32_t recordPoints = scope::RecordBudget::Default; // Total of the enabled channels.
    uint32_t enabled[2] = {1, 1}, coupling[2] = {0, 0}, position[2] = {80, 80};
    uint32_t probeNumerator[2] = {1, 1}, probeDenominator[2] = {1, 1};
    uint32_t triggerSource = 0, triggerEdge = 0, triggerMode = 0;
    int32_t triggerLevel = 0;
    uint32_t triggerPosition=250;
    uint32_t triggerReject=0; // TriggerSettings::reject; files before it: off.
    scope::ReadoutSelection measurements;
    uint32_t runMode = 0; // Index into RunModes.
    uint32_t rollAuto=1;
    bool valid() const;
};
struct MeterPreferences {
    // display: 0 primary, 1 statistics (MEAN), 2 trend, 3 analog dial (appended: 0..2 keep their meaning).
    static constexpr uint32_t Displays = 4, Faces = 3, Palettes = 3;
    uint32_t display = 0, statistics = 1, automatic = 1, range = 0;
    uint32_t face = 0, palette = 0; // ui::MeterStyles: IBM/Mono/Segment, Dark/Amber/Paper.
    // The function (MeterFunctions index) the manual `range` was confirmed in; MeterFunctionCount:
    // unknown (files before it) - such a manual range is not restored, Auto still is.
    uint32_t rangeFunction = MeterFunctionCount;
    // Per-element overrides over palette and face (Measurement > Appearance); none: the base look.
    // The reading's font override, when set, is the reading's face; Default: `face`.
    MeterAppearance appearance;
    bool valid() const {
        return display < Displays && statistics <= 1 && automatic <= 1 && range < 8 && face < Faces && palette < Palettes &&
               rangeFunction <= MeterFunctionCount && appearance.valid();
    }
};
} // namespace openosc
