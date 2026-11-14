#pragma once
#include <cstdint>
namespace openosc {
class Keypad {
public:
    enum Key : uint32_t {
        F1 = 1, F2 = 2, F3 = 4, F4 = 8, Menu = 16,
        Up = 32, Down = 64, Left = 128, Right = 256,
        Mode = 512, HoldSave = 1024, AutoRange = 2048, Relative = 4096
    };
    static void initialize();
    static uint32_t read();
    static uint32_t pressed();
};
}
