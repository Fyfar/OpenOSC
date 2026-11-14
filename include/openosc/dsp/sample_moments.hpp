#pragma once
#include <cstdint>

// Block statistics of signed 8-bit ADC codes: count, sum, sum of squares, minimum and maximum.
// On Cortex-M7 the kernels use the DSP extension (SXTB16 unpacks two codes of one channel from a
// 32-bit read, SMLALD accumulates their squares into 64 bits, SMLAD their sum, SSUB16/SEL their
// extrema); elsewhere an equivalent scalar reference runs. The squares need 64 bits: a full
// 524288-point record of -128 sums to 2^33. Neither kernel converts or copies the record.
namespace openosc::dsp {
struct Moments {
    uint32_t count = 0;
    int64_t sum = 0;
    uint64_t squares = 0;
    int16_t minimum = 127, maximum = -128;
    void add(int16_t code) {
        ++count; sum += code; squares += uint64_t(int32_t(code) * code);
        if (code < minimum) minimum = code;
        if (code > maximum) maximum = code;
    }
    void merge(const Moments &other);
};
// A block accepts at most this many codes, so the per-block 32-bit sums cannot overflow.
inline constexpr uint32_t MaximumBlock = 4096;
// The kernels refuse (accumulate nothing, return false) a block holding a code the ADC bus can
// corrupt (0x7F, 0x3F): the caller then reads that block through the frame's exact correction.
// Every byte is one code (FourByteBanks lanes).
bool accumulateBytes(const uint8_t *bytes, uint32_t count, Moments &moments);
// One channel of 16-bit words: lane 0 is the low byte (CH1), lane 1 the high byte (CH2).
bool accumulateWords(const uint16_t *words, uint32_t count, unsigned lane, Moments &moments);
} // namespace openosc::dsp
