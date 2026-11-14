#pragma once
#include <cstdint>
namespace openosc {
class Handoff {
public:
    [[noreturn]] static void jump(uint32_t vectorAddress);
};
}
