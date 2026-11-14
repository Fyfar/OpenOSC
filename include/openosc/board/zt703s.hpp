#pragma once
#include "openosc/board/capabilities.hpp"
#include "openosc/board/generated/zt703s.hpp"
#include "openosc/board/gpio.hpp"

namespace openosc::board {
struct Zt703s : Zt703sIdentity {
    // Active high on both boards; no internal pull, as the factory's key init (703S 080466A4, KEY4):
    // the power circuit drives the line. The pull-down used before differed from it on the REL path.
    static constexpr InputPin PowerKey{{Port::C, 10}, Pull::None};
    static constexpr Pin ChargeStatus{Port::C, 11};
    static constexpr Capabilities Features{GeneratorDriver::Dac, true, false};
};
} // namespace openosc::board
