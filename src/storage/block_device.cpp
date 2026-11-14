#include "openosc/storage/block_device.hpp"
#include "openosc/platform/board.hpp"
#include <cstring>

namespace openosc {
namespace {
uint16_t little16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t little32(const uint8_t* p) { return little16(p) | (uint32_t(little16(p + 2)) << 16); }
uint32_t bootSectorSize(const uint8_t* p) {
    if (p[510] != 0x55 || p[511] != 0xaa) return 0;
    unsigned bytes = little16(p + 11), cluster = p[13];
    if (bytes < 512 || bytes > 4096 || (bytes & (bytes - 1))) return 0;
    if (!cluster || (cluster & (cluster - 1)) || !little16(p + 14) || !p[16]) return 0;
    return bytes;
}
}

bool BlockDevice::initialize() {
    if (!nor_.read(0, scratch_, 512)) return false;
    if (uint32_t size = bootSectorSize(scratch_)) { sectorSize_ = size; return true; }
    // Preserve a valid existing MBR; inspect its partition using candidate LBA units.
    if (scratch_[510] == 0x55 && scratch_[511] == 0xaa) {
        uint32_t first = little32(scratch_ + 454);
        for (uint32_t size = 512; size <= 4096; size <<= 1) {
            uint64_t offset = uint64_t(first) * size;
            if (first && offset + 512 <= nor_.geometry().capacity
                && nor_.read(offset, scratch_, 512) && bootSectorSize(scratch_) == size) {
                sectorSize_ = size;
                return true;
            }
        }
    }
    sectorSize_ = 512;
    return true; // No formatting: expose the actual raw medium to the host.
}

bool BlockDevice::valid(uint32_t lba, uint32_t count, MediaOwner requester) const {
    return requester == owner_ && lba <= sectorCount() && count <= sectorCount() - lba;
}
bool BlockDevice::read(uint32_t lba, uint32_t count, uint8_t* data, MediaOwner requester) {
    if (!valid(lba, count, requester) || (!data && count)) return false;
    return !count || nor_.read(lba * sectorSize_, data, size_t(count) * sectorSize_);
}
bool BlockDevice::write(uint32_t lba, uint32_t count, const uint8_t* data, MediaOwner requester) {
    if (!valid(lba, count, requester) || (!data && count)) return false;
    uint32_t address = lba * sectorSize_;
    uint32_t remaining = count * sectorSize_;
    while (remaining) {
        const uint32_t physical = address & ~4095U;
        const uint32_t offset = address & 4095U;
        uint32_t chunk = 4096 - offset;
        if (chunk > remaining) chunk = remaining;
        if (!nor_.read(physical, scratch_, sizeof scratch_)) return false;
        std::memcpy(scratch_ + offset, data, chunk);
        if (!nor_.writeSector(physical, scratch_)) return false;
        address += chunk;
        data += chunk;
        remaining -= chunk;
        Board::service();
    }
    return true;
}
bool BlockDevice::sync() { return nor_.ready(); }
}

