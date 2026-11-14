#pragma once
#include <cstdint>

namespace openosc {
class ControlLatch {
public:
    static void write(uint16_t value);
};
} // namespace openosc
