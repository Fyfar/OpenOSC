#include "openosc/dsp/sample_moments.hpp"
#include <cstring>
#if defined(__ARM_FEATURE_DSP)
#include "stm32h7xx.h" // CMSIS-Core: the Cortex-M7 SIMD intrinsics.
#define OPENOSC_DSP_SIMD 1
#elif defined(OPENOSC_DSP_SIMD_EMULATION)
#include "simd_emulation.hpp" // Native test only: the same intrinsics, modelled on a host.
#define OPENOSC_DSP_SIMD 1
#endif

namespace openosc::dsp {
namespace {
inline bool suspect(uint8_t code) { return (code | 0x40U) == 0x7FU; }
inline uint32_t load(const void *p) { uint32_t v; std::memcpy(&v, p, 4); return v; } // LDR; M7 allows unaligned.
// Bit 7 of every byte of x equal to 0x7F or 0x3F (and possibly of a 0x01 byte just above one): the
// usual zero-byte test on x|0x40 ^ 0x7F. An extra flag only sends the block to the exact path.
inline uint32_t suspectBytes(uint32_t x) {
    const uint32_t z = (x | 0x40404040U) ^ 0x7F7F7F7FU;
    return (z - 0x01010101U) & ~z & 0x80808080U;
}
// The same test on one channel's codes of two words (bytes 0 and 2 of codes): a halfword is zero
// only where that channel's code is suspect, and a borrow reaches the upper one only from a real one.
inline uint32_t suspectLane(uint32_t codes) {
    const uint32_t z = (codes | 0x00400040U) ^ 0x007F007FU;
    return (z - 0x00010001U) & ~z & 0x80008000U;
}
inline int16_t low(uint32_t packed) { return int16_t(packed & 0xffffU); }
inline int16_t high(uint32_t packed) { return int16_t(packed >> 16); }

#if defined(OPENOSC_DSP_SIMD)
// Two int16 codes per register in each accumulator. SSUB16 sets the GE flags SEL reads; both are
// volatile asm in CMSIS, so the compiler keeps their order. Locals, not a struct, so that under
// the image's -Os the six accumulators stay in registers.
struct Packed { uint64_t squares; uint32_t sum, maximum, minimum, count; };
void into(Moments &m, const Packed &p) {
    Moments block;
    block.count = p.count; block.sum = int32_t(p.sum); block.squares = p.squares;
    block.maximum = high(p.maximum) > low(p.maximum) ? high(p.maximum) : low(p.maximum);
    block.minimum = high(p.minimum) < low(p.minimum) ? high(p.minimum) : low(p.minimum);
    m.merge(block);
}
#define OPENOSC_DSP_ADD(codes) do { \
        squares = __SMLALD(codes, codes, squares); sum = __SMLAD(codes, 0x00010001U, sum); \
        __SSUB16(codes, maximum); maximum = __SEL(codes, maximum); \
        __SSUB16(minimum, codes); minimum = __SEL(codes, minimum); } while (0)
// Whole 32-bit groups only; false if a suspect code is among them.
__attribute__((noinline)) bool bytesKernel(const uint8_t *bytes, uint32_t groups, Packed &out) {
    uint64_t squares = 0;
    uint32_t sum = 0, maximum = 0x80008000U, minimum = 0x7fff7fffU, flags = 0;
    for (const uint8_t *end = bytes + 4 * groups; bytes != end; bytes += 4) {
        const uint32_t x = load(bytes);
        flags |= suspectBytes(x);
        const uint32_t even = __SXTB16(x), odd = __SXTB16(__ROR(x, 8)); // Codes 0, 2 and 1, 3.
        OPENOSC_DSP_ADD(even);
        OPENOSC_DSP_ADD(odd);
    }
    out = {squares, sum, maximum, minimum, 4 * groups};
    return !flags;
}
__attribute__((noinline)) bool wordsKernel(const uint16_t *words, uint32_t pairs, uint32_t shift, Packed &out) {
    uint64_t squares = 0;
    uint32_t sum = 0, maximum = 0x80008000U, minimum = 0x7fff7fffU, flags = 0;
    for (const uint16_t *end = words + 2 * pairs; words != end; words += 2) {
        const uint32_t x = __ROR(load(words), shift); // The channel's codes in bytes 0 and 2.
        flags |= suspectLane(x & 0x00FF00FFU);
        const uint32_t codes = __SXTB16(x);
        OPENOSC_DSP_ADD(codes);
    }
    out = {squares, sum, maximum, minimum, 2 * pairs};
    return !flags;
}
#undef OPENOSC_DSP_ADD
#endif
} // namespace

void Moments::merge(const Moments &other) {
    if (!other.count) return;
    count += other.count; sum += other.sum; squares += other.squares;
    if (other.minimum < minimum) minimum = other.minimum;
    if (other.maximum > maximum) maximum = other.maximum;
}

#if defined(OPENOSC_DSP_SIMD)
bool accumulateBytes(const uint8_t *bytes, uint32_t count, Moments &moments) {
    if (count > MaximumBlock) return false;
    Packed packed;
    if (!bytesKernel(bytes, count / 4, packed)) return false;
    Moments tail;
    for (uint32_t i = packed.count; i < count; ++i) {
        if (suspect(bytes[i])) return false;
        tail.add(int8_t(bytes[i]));
    }
    // An even number of codes went through each half: the halves' sentinels never win.
    into(moments, packed);
    moments.merge(tail);
    return true;
}
bool accumulateWords(const uint16_t *words, uint32_t count, unsigned lane, Moments &moments) {
    if (count > MaximumBlock || lane > 1) return false;
    Packed packed;
    if (!wordsKernel(words, count / 2, 8 * lane, packed)) return false;
    Moments tail;
    if (count & 1) {
        const uint8_t code = uint8_t(words[count - 1] >> (8 * lane));
        if (suspect(code)) return false;
        tail.add(int8_t(code));
    }
    into(moments, packed);
    moments.merge(tail);
    return true;
}
#else
// Scalar reference: the same refusal rule and results, for hosts without the DSP extension.
bool accumulateBytes(const uint8_t *bytes, uint32_t count, Moments &moments) {
    if (count > MaximumBlock) return false;
    for (uint32_t i = 0; i < count; ++i) if (suspect(bytes[i])) return false;
    Moments block;
    for (uint32_t i = 0; i < count; ++i) block.add(int8_t(bytes[i]));
    moments.merge(block);
    return true;
}
bool accumulateWords(const uint16_t *words, uint32_t count, unsigned lane, Moments &moments) {
    if (count > MaximumBlock || lane > 1) return false;
    for (uint32_t i = 0; i < count; ++i) if (suspect(uint8_t(words[i] >> (8 * lane)))) return false;
    Moments block;
    for (uint32_t i = 0; i < count; ++i) block.add(int8_t(uint8_t(words[i] >> (8 * lane))));
    moments.merge(block);
    return true;
}
#endif
} // namespace openosc::dsp
