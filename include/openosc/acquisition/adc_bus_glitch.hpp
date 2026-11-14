#pragma once
#include <cstdint>
#include <cstdlib>

namespace openosc::scope {
// The ADC output bits do not switch together. A read that lands inside the
// 0xFF<->0x00 transition around 0 V can return 0x7F or 0x3F: one half- or full-scale
// sample between samples at zero. It carries no measurement, yet it would show up as
// a false spike, a false CLIP and a jump in Vpp. Readers resolve it as they read:
// scanning the uncached capture memory separately would cost about 15% of the frame rate.
class AdcBusGlitch {
public:
    static constexpr int NearZero=8;
    __attribute__((always_inline)) static constexpr bool suspect(uint8_t code) { return code==0x7F || code==0x3F; }
    // Either byte of a 16-bit capture word (CH1 low, CH2 high) may be suspect: the zero-byte test
    // of (code|0x40)^0x7F. The upper two bytes of the zero-extended word give 0x3F, never zero; a
    // borrow reaches a higher byte only from a real zero below, so no suspect byte is missed. A flag
    // only sends the point to suspect() and resolve(). Replicated constants: Thumb-2 immediates.
    __attribute__((always_inline)) static constexpr bool suspectWord(uint16_t word) {
        const uint32_t z = (word | 0x40404040U) ^ 0x7F7F7F7FU;
        return ((z - 0x01010101U) & ~z & 0x80808080U) != 0;
    }
    // A clipped or strong real signal stands: its neighbours are not at zero.
    // At the end of a record the one existing neighbour is passed as both.
    static bool stands(uint8_t before,uint8_t after) {
        return std::abs(int(int8_t(before)))>NearZero || std::abs(int(int8_t(after)))>NearZero;
    }
    static uint8_t resolve(uint8_t code,uint8_t before,uint8_t after) {
        if (stands(before,after)) return code;
        return uint8_t((int(int8_t(before))+int(int8_t(after)))/2);
    }
};
} // namespace openosc::scope
