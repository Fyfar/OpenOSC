#include "openosc/generator/waveform.hpp"
#include <cmath>
#include <cstdio>
#include <numeric>
#include <limits>

namespace openosc::generator {
namespace {
unsigned dacPoints(const Settings &settings,uint32_t clock) {
    const double frequency=settings.frequencyHz.value();
    unsigned upper=frequency<=DacCapabilities::DacSamplesPerSecond/1024.0 ? 1024 :
        unsigned(DacCapabilities::DacSamplesPerSecond/frequency);
    if (settings.shape==Shape::Triangle) upper &= ~1U;
    if (upper<28) return 0;
    unsigned selected=upper;
    // Compare count/divider pairs before the full PSC/ARR search. At low rates
    // timer quantization is already tiny; one full TimerPlan search suffices.
    if (clock/(frequency*upper)<=65536) {
        double best=std::numeric_limits<double>::infinity();
        const uint32_t minimum=(uint64_t(clock)+DacCapabilities::DacSamplesPerSecond-1)/DacCapabilities::DacSamplesPerSecond;
        for (unsigned count=upper; count>=28; --count) {
            if (settings.shape==Shape::Triangle && (count&1)) continue;
            const long divider=std::lround(clock/(frequency*count));
            if (divider<long(minimum) || divider>65536) continue;
            const double error=std::abs(double(clock)/(divider*count)-frequency);
            if (error>=best) continue;
            best=error; selected=count;
            if (!error) break;
        }
    }
    return selected;
}
}
bool Settings::valid() const {
    return static_cast<unsigned>(shape) <= static_cast<unsigned>(Shape::Sawtooth) && frequencyHz.valid() &&
           unsigned(level) <= unsigned(Level::Custom) && unsigned(squareDrive) <= unsigned(SquareDrive::Pwm) &&
           minimumCode <= maximumCode && maximumCode <= 4095 && dutyPermille <= 1000 && amplitudeMillivolts <= 5000;
}
CodeRange Levels::codes(const Settings &s) {
    if (s.level == Level::Custom) return {s.minimumCode,s.maximumCode};
    if (s.level == Level::Low) return {0,uint16_t(s.shape==Shape::Sine ? 1600 : 1500)};
    if (s.level == Level::Dedicated && s.shape==Shape::Sine && kilohertz(s))
        return {600,3600};
    return {0,4000};
}
const char *Levels::label(Level level) {
    return level == Level::Low ? "1.0" : level == Level::Custom ? nullptr : "2.5";
}
void Levels::describe(const Settings &s, char *output, size_t capacity) {
    if (!capacity) return;
    const char *caption = label(s.level);
    const auto range = codes(s);
    if (!caption) std::snprintf(output, capacity, "Custom DAC %u-%u", range.minimum, range.maximum);
    else if (pwm(s)) std::snprintf(output, capacity, "PWM logic level, factory label %sV", caption);
    else std::snprintf(output, capacity, "DAC %u-%u, factory label %sV", range.minimum, range.maximum, caption);
}
bool WaveTable::build(const Settings &settings, uint16_t *samples, size_t count) {
    if (!settings.valid() || !samples || count < 2 || count > 4096)
        return false;
    const auto codes=Levels::codes(settings);
    const double span = codes.maximum - codes.minimum;
    constexpr double pi = 3.1415926535897932384626433832795;
    for (size_t i = 0; i < count; ++i) {
        const double phase = double(i) / count;
        double normalized = 0;
        switch (settings.shape) {
        case Shape::Sine:
            normalized = .5 + .5 * std::sin(2 * pi * phase);
            break;
        case Shape::Square:
            normalized = i < (uint64_t(count)*settings.dutyPermille+500)/1000 ? 1 : 0;
            break;
        case Shape::Triangle:
            normalized = phase < .5 ? 2 * phase : 2 - 2 * phase;
            break;
        case Shape::HalfSine:
            normalized = phase < .5 ? std::sin(2 * pi * phase) : 0;
            break;
        case Shape::PositiveSine:
            normalized = std::sin(pi * phase);
            break;
        case Shape::Sawtooth:
            normalized = phase;
            break;
        }
        const long value = std::lround(codes.minimum + span * normalized);
        samples[i] = value < codes.minimum   ? codes.minimum
                     : value > codes.maximum ? codes.maximum
                                                    : value;
    }
    return true;
}
bool OutputPlan::calculate(const Settings &settings, uint32_t clock, OutputPlan &output) {
    if (!clock || !settings.valid() || settings.frequencyHz.value()*1000>DacCapabilities::maximumMillihertz(settings.shape,settings.level))
        return false;
    OutputPlan candidate;
    candidate.pwm = Levels::pwm(settings);
    if (candidate.pwm) {
        if (!TimerPlan::calculate(clock, settings.frequencyHz, candidate.timer, 2))
            return false;
        const uint32_t period = candidate.timer.reload + 1U;
        candidate.compare = (uint64_t(period) * settings.dutyPermille + 500) / 1000;
        if (settings.dutyPermille > 0 && settings.dutyPermille < 1000 &&
            (!candidate.compare || candidate.compare == period))
            return false;
        candidate.actualHz = {candidate.timer.actualHz.numerator, candidate.timer.actualHz.denominator};
    } else {
        candidate.count=dacPoints(settings,clock);
        if (!candidate.count) return false;
        const unsigned high=(unsigned(candidate.count)*settings.dutyPermille+500)/1000;
        if (settings.shape==Shape::Square && settings.dutyPermille && settings.dutyPermille<1000 &&
            (!high || high==candidate.count)) return false;
        uint64_t numerator = uint64_t(settings.frequencyHz.numerator) * candidate.count;
        uint64_t denominator = settings.frequencyHz.denominator;
        const uint64_t common = std::gcd(numerator, denominator);
        numerator /= common;
        denominator /= common;
        if (numerator > UINT32_MAX || denominator > UINT32_MAX ||
            !TimerPlan::calculate(clock, {uint32_t(numerator), uint32_t(denominator)}, candidate.timer,
                (uint64_t(clock)+DacCapabilities::DacSamplesPerSecond-1)/DacCapabilities::DacSamplesPerSecond))
            return false;
        numerator = candidate.timer.actualHz.numerator;
        denominator = uint64_t(candidate.timer.actualHz.denominator) * candidate.count;
        const uint64_t reduced = std::gcd(numerator, denominator);
        candidate.actualHz = {numerator / reduced, denominator / reduced};
    }
    output = candidate;
    return true;
}
} // namespace openosc::generator
