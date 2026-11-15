#pragma once
#include "openosc/board/current.hpp"
#include "openosc/generator/module_wave.hpp"
#include "openosc/generator/frequency.hpp"
#include <cstdio>
#include <type_traits>

namespace openosc::generator {
// ZT706S: the external module on USART3 (ModuleWave). Limits of the factory GEN 706S UI: frequency
// clamp 24071D4C (1 Hz; sine 10 MHz, square/triangle/half/positive 2 MHz, saw 200 kHz), duty 2407105C
// (1..100 %, here in permille as for the DAC; 100 % a constant high), amplitude 0..5 V on the 0.1 V
// grid with the limit of 2406E008.
// Nominal: the module's real clock and output have not been measured.
struct ModuleCapabilities {
    static constexpr bool Levels = false, Amplitude = true;
    static constexpr uint64_t MinimumMillihertz = 1000;
    static constexpr uint16_t MinimumDutyPermille = ModuleWave::MinimumDuty * 10, MaximumDutyPermille = ModuleWave::MaximumDuty * 10,
                              DutyStepPermille = 10, AmplitudeStepMillivolts = 100;
    static uint64_t maximumMillihertz(Shape shape, Level = Level::Dedicated) {
        return shape == Shape::Sine ? 10000000000ULL : shape == Shape::Sawtooth ? 200000000ULL : 2000000000ULL;
    }
    static bool duty(Shape shape) { return shape == Shape::Square; }
    static uint16_t amplitudeLimitMillivolts(Shape shape, uint64_t millihertz) { return ModuleWave::limitMillivolts(shape, millihertz); }
    // The one validation of a setting (driver, SCPI, settings file): what the module can do as asked,
    // nothing clamped silently. A shape whose limit is below the frequency or amplitude already set
    // is refused (the UI editors clamp first and say so), and a square whose duty no sample count at
    // this frequency holds (1 % needs 50 samples or more).
    static bool accepts(const Settings &s) {
        const uint64_t millihertz = Frequency::millihertz(s.frequencyHz);
        return s.valid() && millihertz >= MinimumMillihertz && millihertz <= maximumMillihertz(s.shape) &&
               s.dutyPermille >= MinimumDutyPermille && s.dutyPermille <= MaximumDutyPermille &&
               !(s.amplitudeMillivolts % AmplitudeStepMillivolts) && // The factory's 0.1 V grid, everywhere.
               s.amplitudeMillivolts <= amplitudeLimitMillivolts(s.shape, millihertz) &&
               ModuleWave::timing(millihertz, s.shape, s.dutyPermille).count;
    }
};
using Capabilities = std::conditional_t<board::Current::Features.generator == board::GeneratorDriver::SerialModule,
                                        ModuleCapabilities, DacCapabilities>;
// What sets the output level, in words for the generator screen: ZT703S its level profile
// (Levels), ZT706S the module's amplitude as set, nominal (its output has not been measured).
struct LevelText {
    static void name(const Settings &s, char *output, size_t capacity) { // "Dedicated", "3.0 V".
        if (Capabilities::Levels) std::snprintf(output, capacity, "%s", Levels::Names[unsigned(s.level) % 4]);
        else std::snprintf(output, capacity, "%u.%u V", s.amplitudeMillivolts / 1000, s.amplitudeMillivolts % 1000 / 100);
    }
    static void describe(const Settings &s, char *output, size_t capacity) {
        if (Capabilities::Levels) Levels::describe(s, output, capacity);
        else std::snprintf(output, capacity, "Amplitude %u.%u V, module nominal", s.amplitudeMillivolts / 1000,
                           s.amplitudeMillivolts % 1000 / 100);
    }
};
} // namespace openosc::generator
