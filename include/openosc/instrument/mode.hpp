#pragma once
#include <cstdint>

namespace openosc {
enum class MeasurementMode : uint8_t { Oscilloscope, Multimeter, Generator, Count };
inline constexpr unsigned InstrumentModeCount = static_cast<unsigned>(MeasurementMode::Count);
inline constexpr const char* InstrumentModeKeys[] = {"oscilloscope", "multimeter", "generator"};
static_assert(sizeof InstrumentModeKeys / sizeof *InstrumentModeKeys == InstrumentModeCount);
}
