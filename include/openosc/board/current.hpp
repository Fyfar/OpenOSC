#pragma once
// The only place that turns the build-selected board into code.
#if !defined(OPENOSC_BOARD)
#error "OPENOSC_BOARD must be defined: 703 or 706"
#elif OPENOSC_BOARD == 703
#include "openosc/board/zt703s.hpp"
namespace openosc::board {
using Current = Zt703s;
}
#elif OPENOSC_BOARD == 706
#include "openosc/board/zt706s.hpp"
namespace openosc::board {
using Current = Zt706s;
}
#else
#error "Unsupported OPENOSC_BOARD: expected 703 or 706"
#endif
