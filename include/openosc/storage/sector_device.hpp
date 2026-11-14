#pragma once
#include <cstdint>

namespace openosc {
class SectorDevice {
public:
    virtual uint32_t sectorSize() const = 0;
    virtual uint32_t sectorCount() const = 0;
    virtual bool read(uint32_t sector, uint8_t* data) = 0;
    virtual bool write(uint32_t sector, const uint8_t* data) = 0;
    virtual bool sync() = 0;
protected:
    ~SectorDevice() = default;
};
}
