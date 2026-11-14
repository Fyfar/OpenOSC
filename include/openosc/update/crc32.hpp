#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class Crc32 {
public:
    void update(const void* data, size_t size);
    uint32_t value() const { return state_ ^ 0xffffffffU; }
    static uint32_t compute(const void* data, size_t size);
private:
    uint32_t state_ = 0xffffffffU;
};
}

