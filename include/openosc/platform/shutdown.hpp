#pragma once
#include <cstdint>

namespace openosc {
enum class ShutdownAction : uint8_t { None, PowerOff, Recovery, Restart };
}
