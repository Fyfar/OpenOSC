#pragma once
#include "openosc/board/capabilities.hpp"
#include "openosc/board/generated/zt706s.hpp"
#include "openosc/board/gpio.hpp"

namespace openosc::board {
// Not yet run on hardware. See docs/BOARDS.md.
struct Zt706s : Zt706sIdentity {
    // Active high on both boards.
    static constexpr InputPin PowerKey{{Port::A, 6}, Pull::None};
    static constexpr Pin ChargeStatus{Port::C, 6};
    // The generator is an external module on USART3 (PC10 TX / PC11 RX, generator::ModuleOutput); PA6
    // is the power key and PC6 the charge-status input, so neither 703S output driver runs here.
    // DMM: 66000 counts (ZT-706S manual, multimeter specifications, pp. 31-34).
    static constexpr Capabilities Features{GeneratorDriver::SerialModule, false, true, DmmCounts::Counts66000};
};
} // namespace openosc::board
