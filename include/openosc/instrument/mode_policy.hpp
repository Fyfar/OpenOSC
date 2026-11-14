#pragma once
#include "openosc/instrument/mode.hpp"

namespace openosc {
// Scope -> DMM -> GEN -> Scope: every board has all three (the ZT706S generator is its USART3 module).
constexpr MeasurementMode nextMode(MeasurementMode mode) {
    return static_cast<MeasurementMode>((static_cast<unsigned>(mode) + 1) % InstrumentModeCount);
}
} // namespace openosc
