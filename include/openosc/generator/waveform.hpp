#pragma once
#include "openosc/platform/timer_plan.hpp"
#include <cstddef>

namespace openosc::generator {
enum class Shape : uint8_t { Sine, Square, Triangle, HalfSine, PositiveSine, Sawtooth };
enum class Level : uint8_t { Dedicated, Low, High, Custom };
struct Settings {
    Shape shape = Shape::Sine;
    Level level = Level::Dedicated;
    Rational frequencyHz{1000, 1};
    uint16_t minimumCode = 1000, maximumCode = 3000, dutyPermille = 500;
    uint16_t amplitudeMillivolts = 3000; // ZT706S module only (factory default 3.0 V); ZT703S levels use codes.
    bool valid() const;
    bool same(const Settings &other) const {
        return shape == other.shape && level == other.level && minimumCode == other.minimumCode && maximumCode == other.maximumCode &&
            dutyPermille == other.dutyPermille && amplitudeMillivolts == other.amplitudeMillivolts && uint64_t(frequencyHz.numerator) * other.frequencyHz.denominator ==
                uint64_t(other.frequencyHz.numerator) * frequencyHz.denominator;
    }
};
struct CodeRange { uint16_t minimum, maximum; };
class Levels {
public:
    static CodeRange codes(const Settings &settings);
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
