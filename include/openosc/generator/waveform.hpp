#pragma once
#include "openosc/platform/timer_plan.hpp"
#include <cstddef>

namespace openosc::generator {
enum class Shape : uint8_t { Sine, Square, Triangle, HalfSine, PositiveSine, Sawtooth };
enum class Level : uint8_t { Dedicated, Low, High, Custom };
// How a Dedicated square leaves the ZT703S: Factory as GEN 703S apply 2406E65C (DAC samples 0/4000
// below 1 kHz, TIM13 PWM from 1 kHz), or Pwm at every frequency (no DAC stream, exact duty).
enum class SquareDrive : uint8_t { Factory, Pwm };
struct Settings {
    Shape shape = Shape::Sine;
    Level level = Level::Dedicated;
    SquareDrive squareDrive = SquareDrive::Factory;
    Rational frequencyHz{1000, 1};
    uint16_t minimumCode = 1000, maximumCode = 3000, dutyPermille = 500;
    uint16_t amplitudeMillivolts = 3000; // ZT706S module only (factory default 3.0 V); ZT703S levels use codes.
    bool valid() const;
    bool same(const Settings &other) const {
        return shape == other.shape && level == other.level && squareDrive == other.squareDrive && minimumCode == other.minimumCode && maximumCode == other.maximumCode &&
            dutyPermille == other.dutyPermille && amplitudeMillivolts == other.amplitudeMillivolts && uint64_t(frequencyHz.numerator) * other.frequencyHz.denominator ==
                uint64_t(other.frequencyHz.numerator) * frequencyHz.denominator;
    }
};
struct CodeRange { uint16_t minimum, maximum; };
// ZT703S level profiles: the DAC codes of the factory firmware's two generator paths (zt_fw
// docs/subsystems/generator.md, main_dac_output.md). Dedicated: GEN 703S V1.6.27 builders (sine
// sin*2000+2000 below 1 kHz, sin*1500+2100 from 1 kHz; the others 0..4000; square in kHz on TIM13
// PWM). Low/High: main's OUTPUT level 1/0 (sine 800/2000 offset and scale, square 1500/4000 after
// its duty pass, triangle steps 3/8 to 1500/4000); OpenOSC applies them to all six shapes. Custom:
// the user's codes. Nothing here is calibrated: the factory has no generator calibration (its
// calibration record 0801C000 holds scope offsets, screen gains and the interleave correction).
class Levels {
public:
    static CodeRange codes(const Settings &settings);
    // GEN 703S's kHz units (frequency controller 2406B808: 999 Hz steps up to 1 kHz): the sine's
    // 600..3600 and the factory square's PWM start here.
    static constexpr double KilohertzFrom = 1000;
    static bool kilohertz(const Settings &settings) { return settings.frequencyHz.value() >= KilohertzFrom; }
    // TIM13 PWM on PA6 (a logic level, no DAC codes): a Dedicated square from 1 kHz (Factory) or at
    // every frequency (Pwm). Every other output is DAC samples. OutputPlan, the driver and the UI
    // text all follow this one decision.
    static bool pwm(const Settings &settings) {
        return settings.shape == Shape::Square && settings.level == Level::Dedicated &&
               (settings.squareDrive == SquareDrive::Pwm || kilohertz(settings));
    }
    static constexpr const char *DriveNames[] = {"Factory", "PWM"};
    // The factory's fixed caption of a profile, volts as text ("2.5": GEN 703S "VPP : 2.5V" and
    // main OUTPUT level 0; "1.0": level 1; nullptr: Custom). A UI text, never a measured amplitude.
    static const char *label(Level level);
    // The level as it is applied, for the UI: "DAC 600-3600, factory label 2.5V", "PWM logic level,
    // factory label 2.5V", "Custom DAC 123-2048".
    static void describe(const Settings &settings, char *output, size_t capacity);
    static constexpr const char *Names[] = {"Dedicated", "Low", "High", "Custom"};
};
inline constexpr const char *ShapeNames[] = {"Sine", "Square", "Triangle", "Half sine", "Positive sine", "Sawtooth"};
// ZT703S: DAC2 + BDMA + TIM6 (TIM13 PWM for the dedicated square) on PA6. Frequencies in millihertz.
struct DacCapabilities {
    static constexpr bool Levels = true, Amplitude = false;
    static constexpr uint64_t MinimumMillihertz = 1;
    static constexpr uint16_t MinimumDutyPermille = 0, MaximumDutyPermille = 1000, DutyStepPermille = 5, AmplitudeStepMillivolts = 0;
    static uint64_t maximumMillihertz(Shape shape, Level level=Level::Dedicated) {
        constexpr uint32_t maxima[]={344000000,1000000000,100000000,129000000,294000000,100000000};
        // A square on the DAC: 100 kHz. A Dedicated square is PWM from 1 kHz up (either drive).
        if (shape==Shape::Square && level!=Level::Dedicated) return 100000000;
        return unsigned(shape)<6 ? maxima[unsigned(shape)] : 0;
    }
    static constexpr uint32_t DacSamplesPerSecond=10000000;
    static bool duty(Shape shape) { return shape == Shape::Square; }
    static uint16_t amplitudeLimitMillivolts(Shape, uint64_t) { return 5000; } // Not used: the module's field is kept as stored.
    // Frequency within the shape and level (the timer plan checks what it can represent).
    static bool accepts(const Settings &s) {
        const double millihertz = s.frequencyHz.value() * 1000;
        return s.valid() && millihertz >= MinimumMillihertz && millihertz <= double(maximumMillihertz(s.shape, s.level));
    }
};
class WaveTable {
public:
    static bool build(const Settings &settings, uint16_t *samples, size_t count);
};
struct OutputPlan {
    bool pwm = false;
    uint16_t count = 0;
    uint32_t compare = 0;
    TimerPlan timer;
    WideRational actualHz;
    static bool calculate(const Settings &settings, uint32_t timerClockHz, OutputPlan &output);
};
struct Snapshot {
    Settings requested;
    OutputPlan applied;
    DataStatus status = DataStatus::NoData;
    bool enabled = false;
};
} // namespace openosc::generator
