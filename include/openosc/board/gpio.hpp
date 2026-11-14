#pragma once
#include <cstdint>

namespace openosc::board {
// HAL-free pin description, so board data stays usable from host tests.
enum class Port : uint8_t { A, B, C, D, E };
enum class Pull : uint8_t { None, Up, Down };
struct Pin {
    Port port;
    uint8_t number;
    constexpr uint32_t mask() const { return 1U << number; }
};
struct InputPin {
    Pin pin;
    Pull pull;
};
} // namespace openosc::board
