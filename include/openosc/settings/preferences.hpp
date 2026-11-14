#pragma once
#include "openosc/settings/run_mode.hpp"
#include "openosc/acquisition/ranges.hpp"
#include "openosc/acquisition/sample_plan.hpp"
#include "openosc/instrument/mode.hpp"
#include "openosc/measurement/meter_function.hpp"
#include "openosc/settings/instrument_preferences.hpp"
#include "openosc/generator/capabilities.hpp"
#include "openosc/platform/idle_policy.hpp"
#include <cstdint>

namespace openosc {
struct Preferences {
    uint32_t brightness = 65, mode = 0;
    uint64_t timebaseNs = 1000000;
    uint32_t millivolts[2] = {500, 500}, meterFunction = 0;
    uint32_t useBuiltinTheme = 0;
    uint32_t showFps = 1, batteryDisplay = 0; // batteryDisplay: 0 icon, 1 volts, 2 percent.
    uint32_t generatorShape = 0, generatorDutyPermille = 500;
    uint64_t generatorMillihertz = 1000000; // uint64: the ZT706S module reaches 10 MHz = 1e10 mHz.
    uint32_t generatorLevel = 0, generatorMinimumCode = 1000, generatorMaximumCode = 3000;
    uint32_t generatorAmplitudeMillivolts = 3000; // ZT706S module; ZT703S keeps level/codes.
    ScopePreferences scope;
    MeterPreferences meter;
    IdleSettings idle;
    bool valid() const {
        scope::SamplePlan sampling;
        return brightness >= 10 && brightness <= 100 && mode < InstrumentModeCount && timebaseNs >= 10 &&
               timebaseNs <= 20000000000ULL && scope::VoltageRanges::contains(millivolts[0]) &&
               scope::VoltageRanges::contains(millivolts[1]) && meterFunction < MeterFunctionCount &&
               useBuiltinTheme <= 1 && showFps <= 1 && batteryDisplay < 3 && generatorShape < 6 &&
               generatorMillihertz >= generator::Capabilities::MinimumMillihertz &&
               generatorMillihertz <= generator::Capabilities::maximumMillihertz(generator::Shape(generatorShape),generator::Level(generatorLevel)) &&
               generatorDutyPermille >= generator::Capabilities::MinimumDutyPermille &&
               generatorDutyPermille <= generator::Capabilities::MaximumDutyPermille &&
               generatorAmplitudeMillivolts <= generator::Capabilities::amplitudeLimitMillivolts(generator::Shape(generatorShape), generatorMillihertz) &&
               generatorLevel<4 && generatorMinimumCode<=generatorMaximumCode &&
               generatorMaximumCode<=4095 && scope.valid() && meter.valid() && idle.valid() &&
               scope::SamplePlan::calculate(timebaseNs,
                    scope::RecordBudget::perChannel(scope.recordPoints,scope::RecordBudget::channels(scope.enabled[0],scope.enabled[1])),
                    RunModes[scope.runMode].maximumSampleRate(),scope::RecordBudget::Maximum,sampling);
    }
};
} // namespace openosc
