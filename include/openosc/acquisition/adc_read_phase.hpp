#pragma once
#include <cstdint>

namespace openosc::scope {
// Where TIM3's GPIOD reads fall within the ADC clock period. Both ADC clocks (TIM1 CH1 and CH3) are always
// in phase with each other; the read phase is their common polarity against TIM3's reads:
// - Factory: the factory polarity (CC1P/CC3P set).
// - Inverted: both outputs inverted. The clock edges then move against the reads by half an ADC period
//   at period 2 or 4 (50 % clock), and by one or two ticks of three (120 or 240 degrees) at period 3
//   (33/67 % clock); which clock edge the converters use is not known here.
// Every plan reads at the factory polarity. At 360 MHz / 90 MS/s (period 4) Root's held records were clean
// in both polarities (1 MHz square at 90 and 45 MS/s, 1 kHz at 1.64 MS/s, DC; 2026-11-13), and the
// factory one was chosen. Earlier top profiles: the rule once measured on 340 MHz (period 2, PSC >= 1:
// inverted) ended with that profile; 320/160 and 330/110 had outliers at the factory polarity.
class AdcReadPhase {
public:
    enum class Phase : uint8_t { Factory, Inverted };
    static constexpr Phase select(uint32_t /*timerHz*/, uint16_t /*prescaler*/, uint16_t /*period*/) { return Phase::Factory; }
};
} // namespace openosc::scope
