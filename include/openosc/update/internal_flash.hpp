#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc {
class InternalFlash {
public:
    bool eraseSector(uint32_t address);
    bool program(uint32_t address, const void* data, size_t size);
    void lock();
    const char* error() const { return error_; }
private:
    bool unlock(uint32_t address);
    bool wait(uint32_t address);
    bool fail(const char* message);
    const char* error_ = nullptr;
};
}

