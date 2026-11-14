#pragma once
#include <cstdint>

namespace openosc {
// The factory firmware picks PLL1 from a stored setting: HSE/5, N=80 or 112, P=2, and every
// APB bus at HCLK/2. The third profile extends that scheme beyond the 280 MHz data sheet limit
// (and beyond the wide PLL1 VCO range of 560 MHz: VCO = 5 MHz x N): 360 MHz / 90 MS/s (divider 4), under
// trial. The slot held 340/170, then in trials on 2026-11-13 320/160, 300/150, 400/100 (rejected) and
// 330/110; a saved index 2 selects whatever the slot holds.
// TIM1 (ADC clock) and TIM3 (reads) run at the core clock; one ADC period lasts at least `adcDivider`
// timer ticks (2, 3 or 4; period 3 gives a 33/67 % clock), so a profile's per-channel maximum is
// coreHz / adcDivider. The ADCs are marked MXT2088 (MXTronics, 100 MSPS, clock levels >= 4.3 ns); 90 MS/s
// at 360 MHz (period 4, compare 2: 5.56/5.56 ns) is within that rating.
struct RunMode {
    uint32_t coreHz, pllN, adcDivider, flashLatency;
    constexpr uint32_t maximumSampleRate() const { return coreHz / adcDivider; }
};
// Above 280 MHz: at least the 7 wait states used before, more where the factory's flash access time
// (280 MHz at 5 wait states: 46.7 MHz) would otherwise be exceeded.
constexpr uint32_t overclockedFlashLatency(uint32_t coreHz) {
    const uint32_t needed = uint32_t((uint64_t(coreHz) * 6 + 279999999) / 280000000) - 1;
    return needed > 7 ? needed : 7;
}
constexpr RunMode overclockedRunMode(uint32_t mhz, uint32_t adcDivider) {
    return {mhz * 1000000, mhz * 2 / 5, adcDivider, overclockedFlashLatency(mhz * 1000000)};
}
inline constexpr RunMode RunModes[] = {{200000000, 80, 2, 5}, {280000000, 112, 2, 5}, overclockedRunMode(360, 4)};
inline constexpr uint32_t RunModeCount = sizeof RunModes / sizeof RunModes[0];
inline constexpr uint32_t OverclockedRunMode = 2;
// The highest per-channel sample rate of any profile (a top profile with a larger divider may be lower).
constexpr uint32_t highestSampleRate() {
    uint32_t highest = 0;
    for (const auto &mode : RunModes) highest = mode.maximumSampleRate() > highest ? mode.maximumSampleRate() : highest;
    return highest;
}
inline constexpr uint32_t MaximumSampleRate = highestSampleRate();
// The ADC divider of the profile running at `coreHz`; 2 for a clock no profile has (the HSI fallback).
constexpr uint32_t adcDividerAt(uint32_t coreHz) {
    for (const auto &mode : RunModes) if (mode.coreHz == coreHz) return mode.adcDivider;
    return 2;
}
constexpr bool validRunModes() {
    for (const auto &mode : RunModes)
        if (mode.coreHz != mode.pllN * 2500000 || mode.adcDivider < 2 || mode.adcDivider > 4 || mode.flashLatency > 15)
            return false;
    return true;
}
static_assert(validRunModes(), "Run modes: HSE/5 x N / 2, ADC divider 2..4, flash latency 0..15");
} // namespace openosc
