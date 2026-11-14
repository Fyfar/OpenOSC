#pragma once
#include "openosc/storage/block_device.hpp"
#include "openosc/storage/sector_device.hpp"

namespace openosc {
class LocalSectors final : public SectorDevice {
public:
    explicit LocalSectors(BlockDevice& disk) : disk_(disk) {}
    uint32_t sectorSize() const override { return disk_.sectorSize(); }
    uint32_t sectorCount() const override { return disk_.sectorCount(); }
    bool read(uint32_t sector, uint8_t* data) override { return disk_.read(sector, 1, data, MediaOwner::Local); }
    bool write(uint32_t sector, const uint8_t* data) override { return disk_.write(sector, 1, data, MediaOwner::Local); }
    bool sync() override { return disk_.sync(); }
private:
    BlockDevice& disk_;
};
}
