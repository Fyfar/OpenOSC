#pragma once
#include <cstdint>

namespace openosc {
class RecoveryRequest {
public:
    static constexpr uint32_t Address = 0x2001ffc0, Magic = 0x52454356;
    static bool take();
    [[noreturn]] static void restart();
};
}
