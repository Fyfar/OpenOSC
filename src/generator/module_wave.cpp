#include "openosc/generator/module_wave.hpp"
#include "openosc/generator/frequency.hpp"
#include <cmath>

// float32 steps as the factory's FPU code (no fused multiply-add: its rounding differs).
#define OPENOSC_FACTORY_FLOAT __attribute__((optimize("fp-contract=off")))

namespace openosc::generator {
namespace {
// VCVT to unsigned, round toward zero, saturating (NaN and negatives give 0).
uint32_t unsignedOf(double value) {
    if (!(value > 0)) return 0;
    return value >= 4294967295.0 ? 0xffffffffU : uint32_t(value);
}
float f32(double value) { return float(value); }
} // namespace

uint16_t ModuleWave::limitMillivolts(Shape shape, uint64_t millihertz) {
    const uint64_t hertz = millihertz / 1000;
    switch (shape) {
    case Shape::Sine:
        return hertz < 1000000 ? 5000 : hertz < 7000000 ? 4000 : hertz < 10000000 ? 3500 : 3000;
    case Shape::Triangle: case Shape::HalfSine: case Shape::PositiveSine:
        return hertz < 1000000 ? 5000 : 4000;
    case Shape::Sawtooth: return 4000;
    case Shape::Square: break;
    }
    return 5000;
}
OPENOSC_FACTORY_FLOAT float ModuleWave::correction(Shape shape, uint32_t hertz, float volts) {
    float raw = 0;
    switch (shape) {
    case Shape::Sine:
        if (hertz < 1000000) break;
        if (hertz < 4000000) raw = float((hertz / 166667) >> 2) * volts;      // Integer steps.
        else if (hertz < 5000000) raw = float((hertz / 133333) >> 2) * volts;
        else if (hertz < 6000000) raw = volts * 10.0f;
        else if (hertz < 7000000) raw = volts * 15.0f;
        else if (hertz < 8000000) raw = volts * 15.714286f;
        else if (hertz < 9000000) raw = volts * 20.0f;
        else if (hertz < 10000000) raw = volts * 24.285715f;
        else raw = volts * 40.0f;
        break;
    case Shape::Square: if (hertz >= 1000000) raw = volts * 3.0f; break;
    case Shape::Triangle: case Shape::PositiveSine: if (hertz >= 1000000) raw = volts * 6.25f; break;
    case Shape::HalfSine: if (hertz >= 1000000) raw = volts * 5.0f; break;
    case Shape::Sawtooth: raw = volts * 5.0f; break;
    }
    return f32(double(raw) * 1.2);
}
OPENOSC_FACTORY_FLOAT uint16_t ModuleWave::code(float volts, float correction) {
    if (volts > 5.0f) volts = 5.0f;
    const uint32_t scaled = unsignedOf(volts * 51.0f);
    uint16_t result = uint16_t(scaled);
    const auto corrected = [&](float coefficient) {
        const uint16_t reduced = uint16_t(unsignedOf(float(uint16_t(scaled)) - volts * coefficient));
        return uint16_t(unsignedOf(float(reduced) + correction));
    };
    if (volts >= 4.0f && volts < 5.0f) result = corrected(12.0f);
    else if (volts >= 3.0f && volts < 4.0f) result = corrected(13.3f);
    else if (volts >= 2.0f && volts < 3.0f) result = corrected(13.0f);
    else if (volts >= 1.0f && volts < 2.0f) result = corrected(15.0f);
    else if (volts >= 0.1f && volts < 1.0f) result = uint16_t(unsignedOf(volts * 42.5f));
    else if (correction == 0.0f) result = uint16_t(result - 57); // 5 V: 198. (0 V wraps: build() never asks.)
    return result > 255 ? 255 : result;
}
OPENOSC_FACTORY_FLOAT ModuleWave::Timing ModuleWave::timing(uint64_t millihertz, unsigned start, bool even) {
    Timing best;
    if (!millihertz) return best;
    if (start > MaximumSamples) start = MaximumSamples;
    if (start < MinimumSamples) start = MinimumSamples;
    const float target = f32(double(millihertz) / 1000.0);
    float bestError = float(0xffffff);
    for (unsigned count = start; count >= MinimumSamples; --count) {
        if (even && (count & 1)) continue;
        // floor(floor(ClockHz / N) / Hz) of the factory, for a target in millihertz.
        uint64_t divisor = uint64_t(ClockHz) * 1000 / (uint64_t(count) * millihertz);
        if (!divisor) divisor = 1;
        if (divisor > 0xffffffffU) divisor = 0xffffffffU;
        const float actual = (float(ClockHz) / float(count)) / float(uint32_t(divisor));
        if (actual == target && !(count & 3)) return {uint32_t(divisor), count};
        const float error = std::fabs(actual - target);
        if (error < bestError) {
            bestError = float(unsignedOf(error)); // The factory keeps it as an integer.
            best = {uint32_t(divisor), count};
        }
        if (millihertz < 500000000ULL && count < 100) break;
    }
    return best;
}
OPENOSC_FACTORY_FLOAT void ModuleWave::samples(Shape shape, unsigned count, uint16_t code, unsigned duty, uint8_t *output) {
    double phase = 0;
    float value = 0;
    const unsigned half = code >> 1;
    for (unsigned i = 0; i < count; ++i) {
        double sample = 0;
        switch (shape) {
        case Shape::Sine:
            sample = double(half) + double(half) * std::sin(phase); phase += 6.28 / count; break;
        case Shape::Square: {
            const float threshold = f32(float(count) / f32(100.0 / double(float(100 - duty))));
            sample = float(i) <= threshold ? 0 : code; break;
        }
        case Shape::Triangle: {
            const float step = f32(float(code) / f32(float(count) / 2.0));
            value = i < count / 2 ? f32(value + step) : f32(value - step); sample = value; break;
        }
        case Shape::HalfSine:
            sample = phase <= 3.14 ? std::sin(phase) * double(half * 2) : 0; phase += 6.28 / count; break;
        case Shape::PositiveSine:
            sample = phase <= 3.14 ? std::sin(phase) * double(code) : 0; phase += 3.14 / count; break;
        case Shape::Sawtooth:
            value = f32(value + f32(float(code) / float(count))); sample = value; break;
        }
        output[i] = uint8_t(unsignedOf(sample));
    }
}
bool ModuleWave::build(const Settings &settings, bool enabled, Frame &frame) {
    frame = {};
    const uint64_t millihertz = Frequency::millihertz(settings.frequencyHz);
    if (!settings.valid() || !millihertz) return false;
    uint8_t wave[MaximumSamples];
    const Shape shape = settings.shape;
    uint64_t target = millihertz;
    unsigned start = MaximumSamples;
    if (enabled && settings.amplitudeMillivolts) {
        const uint16_t limit = limitMillivolts(shape, millihertz);
        const float volts = float(settings.amplitudeMillivolts < limit ? settings.amplitudeMillivolts : limit) / 1000.0f;
        const uint32_t hertz = uint32_t(millihertz / 1000);
        frame.code = code(volts, correction(shape, hertz, volts));
        start = frame.code;
    } else target = 1000000; // A zero waveform at 1 kHz (factory OFF: shape 100 at 1000 Hz).
    frame.timing = timing(target, start, frame.code && shape == Shape::Triangle);
    if (!frame.timing.count || !frame.timing.divisor) return false;
    unsigned duty = settings.dutyPermille / 10;
    duty = duty < MinimumDuty ? MinimumDuty : duty > MaximumDuty ? MaximumDuty : duty;
    if (frame.code) samples(shape, frame.timing.count, frame.code, duty, wave);
    else for (unsigned i = 0; i < frame.timing.count; ++i) wave[i] = 0;
    if (!SerialFrameCodec::encode(wave, frame.timing.count, frame.timing.divisor, frame.bytes, sizeof frame.bytes))
        return false;
    const unsigned checksum = 14 + frame.timing.count;
    frame.acknowledge = uint8_t(frame.bytes[checksum] + frame.bytes[checksum + 1]);
    frame.actualHz = {ClockHz, uint64_t(frame.timing.count) * frame.timing.divisor};
    return true;
}
} // namespace openosc::generator
