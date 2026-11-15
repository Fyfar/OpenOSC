#pragma once
#include "openosc/board/current.hpp"
#include <cstdint>

namespace openosc::generator {
// The control-word bits the generator's output needs while the DMM word is applied (the Scope word
// has them anyway): the board's GeneratorControlBits until the output is confirmed off. An OFF not
// yet acknowledged may still play, and the OFF frame needs the module as it is. ZT706S: without bit
// 10 the module's output disappeared under the DMM (issue #4); ZT703S: none.
constexpr uint16_t controlBits(bool offConfirmed) { return offConfirmed ? 0 : board::Current::GeneratorControlBits; }
} // namespace openosc::generator
