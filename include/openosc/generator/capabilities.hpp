#pragma once
#include "openosc/board/current.hpp"
#include "openosc/generator/module_wave.hpp"
#include "openosc/generator/frequency.hpp"
#include <type_traits>

namespace openosc::generator {
// ZT706S: the external module on USART3 (ModuleWave). Limits of the factory GEN 706S UI: frequency
// clamp 24071D4C (1 Hz; sine 10 MHz, square/triangle/half/positive 2 MHz, saw 200 kHz), duty 2407105C
// (1..100 %, here 1..99: 100 % is a DC level), amplitude 0..5 V by 0.1 V with the limit of 2406E008.
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
    // is refused (the UI editors clamp first and say so).
    static bool accepts(const Settings &s) {
        const uint64_t millihertz = Frequency::millihertz(s.frequencyHz);
        return s.valid() && millihertz >= MinimumMillihertz && millihertz <= maximumMillihertz(s.shape) &&
               s.dutyPermille >= MinimumDutyPermille && s.dutyPermille <= MaximumDutyPermille &&
               s.amplitudeMillivolts <= amplitudeLimitMillivolts(s.shape, millihertz);
    }
};
using Capabilities = std::conditional_t<board::Current::Features.generator == board::GeneratorDriver::SerialModule,
                                        ModuleCapabilities, DacCapabilities>;
} // namespace openosc::generator
