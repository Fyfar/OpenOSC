#pragma once
#include "openosc/storage/nor_flash.hpp"

namespace openosc {
enum class MediaOwner : uint8_t { Local, Host };

class BlockDevice {
public:
    explicit BlockDevice(NorFlash& nor) : nor_(nor) {}
    bool initialize();
    bool read(uint32_t lba, uint32_t count, uint8_t* data, MediaOwner requester);
    bool write(uint32_t lba, uint32_t count, const uint8_t* data, MediaOwner requester);
    bool sync();
    uint32_t sectorSize() const { return sectorSize_; }
    uint32_t sectorCount() const { return nor_.geometry().capacity / sectorSize_; }
    MediaOwner owner() const { return owner_; }
    void setOwner(MediaOwner owner) { owner_ = owner; }
    const char* error() const { return nor_.error(); }
private:
    bool valid(uint32_t lba, uint32_t count, MediaOwner requester) const;
    NorFlash& nor_;
    uint32_t sectorSize_ = 512;
    volatile MediaOwner owner_ = MediaOwner::Local;
    alignas(4) uint8_t scratch_[4096];
};
void registerFatDevice(BlockDevice* device);
}

