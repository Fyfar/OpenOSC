#pragma once
#include <cstdint>

namespace openosc::board {
// The generator each board has: ZT703S DAC2/TIM13 on PA6, ZT706S an external module on USART3.
enum class GeneratorDriver : uint8_t { Dac, SerialModule };
// What this build can actually drive. Installed hardware is not enough: a feature
// stays off until its driver exists and its contract is known.
struct Capabilities {
    GeneratorDriver generator;
    bool dmmSerialOutput;
    // ZT706S: CH1 through both ADCs in opposite clock phase (control bits 8/11, TIM1 CC3P) at twice
    // the word rate, the factory's 10/25 ns interleave. The ZT703S updater (08049B10) is empty.
    bool adcInterleave;
};
} // namespace openosc::board
