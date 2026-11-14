#pragma once
#include "openosc/generator/waveform.hpp"
#include "openosc/generator/frame_codec.hpp"
#include <cstdint>

namespace openosc::generator {
// The ZT706S external generator module's waveform, as the factory GEN 706S V1.6.31 computes it
// (zt_fw docs/subsystems/generator.md, generator_controls.md; addresses below are 706S GEN). The
// module plays N one-byte samples, each held `divisor` ticks of a clock the factory takes as 200 MHz
// (a software constant: the module's real clock and output voltage are not established). OpenOSC
// keeps the factory numbers except where they are unsafe (each noted at its function):
//  - amplitude 0 V is a zero waveform (the factory wraps it to full scale);
//  - the sample-count search starts at least at 20, so N and divisor are never 0 (the factory starts
//    at the amplitude code and sends N=0/divisor=0 below code 20);
//  - the limit follows shape and frequency both ways (the factory only lowers it), and the
//    correction is computed from the amplitude already clamped to it;
//  - a triangle searches even sample counts (the factory drops an odd N by one without a new divisor).
class ModuleWave {
public:
    static constexpr uint32_t ClockHz = 200000000;      // Factory constant, nominal.
    static constexpr unsigned MinimumSamples = 20, MaximumSamples = SerialFrameCodec::MaximumSamples;
    static constexpr uint16_t MaximumMillivolts = 5000;  // Factory amplitude UI 0..5 V, 0.1 V steps.
    static constexpr uint8_t MinimumDuty = 1, MaximumDuty = 99; // Percent (factory 1..100: 100 is DC).
    struct Timing { uint32_t divisor = 0; unsigned count = 0; };
    struct Frame {
        uint8_t bytes[SerialFrameCodec::Bytes]{};
        uint8_t acknowledge = 0;   // Folded checksum: (low + high) of the frame's sum, as the factory main.
        Timing timing;
        uint16_t code = 0;         // Full-scale sample value (0: a zero waveform).
        WideRational actualHz;     // Nominal: ClockHz / (count * divisor).
    };
    // Upper amplitude for a shape and frequency, millivolts (2406E008's caps: 5, 4, 3.5, 3 V).
    static uint16_t limitMillivolts(Shape shape, uint64_t millihertz);
    // Pre-correction in code units for the clamped amplitude (2406E008, float32 then x1.2 in double).
    static float correction(Shape shape, uint32_t hertz, float volts);
    // Full-scale code for the clamped amplitude and its correction (2407178C).
    static uint16_t code(float volts, float correction);
    // Divisor and sample count for `millihertz`, candidates from `start` down to 20 (240707E0 search).
    static Timing timing(uint64_t millihertz, unsigned start, bool even);
    // `count` samples of `shape` up to `code` (240707E0 builders); `duty` in percent for the square.
    static void samples(Shape shape, unsigned count, uint16_t code, unsigned duty, uint8_t *output);
    // The frame for `settings` (OFF: `enabled` false, a zero waveform at 1 kHz as the factory's OFF).
    static bool build(const Settings &settings, bool enabled, Frame &frame);
};
} // namespace openosc::generator
