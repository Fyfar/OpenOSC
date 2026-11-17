#pragma once
#include <cstdint>

namespace openosc::decoding {
// The Cortex-M7 SIMD byte compare the threshold fast path needs, and its exact host emulation for the
// native tests (the DSP extension is instructions of the same core: no coprocessor, no NEON/Helium).
struct Simd {
    static constexpr uint32_t replicate(int8_t value) { return uint32_t(uint8_t(value)) * 0x01010101u; }
    // Per byte: 0xFF where int8(a) - int8(b) >= 0, else 0 (SSUB8 sets GE, SEL picks).
    static inline uint32_t atLeast(uint32_t a, uint32_t b) {
#if defined(__ARM_FEATURE_DSP)
        uint32_t result;
        asm volatile("ssub8 %0, %1, %2\n\tsel %0, %3, %4" : "=&r"(result) : "r"(a), "r"(b), "r"(0xFFFFFFFFu), "r"(0u) : "cc");
        return result;
#else
        uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const int difference = int(int8_t(a >> (8 * i))) - int(int8_t(b >> (8 * i)));
            if (difference >= 0) result |= 0xFFu << (8 * i);
        }
        return result;
#endif
    }
    // Bytes that may be AdcBusGlitch suspects (0x7F, 0x3F): the zero-byte test of (code | 0x40) ^ 0x7F
    // per byte (AdcBusGlitch::suspectWord's, four bytes). A borrow can only flag a byte above a real
    // suspect: false positives only, which send the block to the scalar path.
    static constexpr uint32_t suspects(uint32_t word) {
        const uint32_t z = (word | 0x40404040u) ^ 0x7F7F7F7Fu;
        return (z - 0x01010101u) & ~z & 0x80808080u;
    }
};
} // namespace openosc::decoding
