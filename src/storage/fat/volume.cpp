#include "openosc/storage/fat/volume.hpp"
#include <cstring>

namespace openosc::fat {
void Volume::put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
bool Volume::fail(const char* message) { error_ = message; return false; }
bool Volume::read(uint32_t sector, uint8_t* data) {
    return (sector < disk_.sectorCount() && disk_.read(sector, data)) || fail("FAT SECTOR READ FAILED");
}
bool Volume::write(uint32_t sector, const uint8_t* data) {
    if (sector < geometry_.start || sector - geometry_.start >= geometry_.sectors)
        return fail("WRITE OUTSIDE FAT VOLUME REJECTED");
    if (!disk_.write(sector, data) || !disk_.sync() || !disk_.read(sector, verify_)
        || std::memcmp(data, verify_, disk_.sectorSize())) return fail("FAT WRITE VERIFY FAILED");
    return true;
}
bool Volume::flush() {
    if (!dirty_) return true;
    if (!write(cachedSector_, cache_)) return false;
    dirty_ = false;
    return true;
}
bool Volume::finish() { return flush() && (disk_.sync() || fail("FAT SYNC FAILED")); }
bool Volume::load(uint32_t sector) {
    if (sector == cachedSector_) return true;
    if (!flush() || !read(sector, cache_)) return false;
    cachedSector_ = sector;
    return true;
}
bool Volume::byte(uint32_t address, uint8_t& value) {
    if (!load(address / geometry_.sectorBytes)) return false;
    value = cache_[address % geometry_.sectorBytes];
    return true;
}
bool Volume::put(uint32_t address, uint8_t value) {
    if (!load(address / geometry_.sectorBytes)) return false;
    auto& target = cache_[address % geometry_.sectorBytes];
    if (target != value) { target = value; dirty_ = true; }
    return true;
}
bool Volume::entry(uint32_t cluster, uint32_t& value) {
    if (cluster >= geometry_.clusters + 2) return fail("FAT CLUSTER OUT OF RANGE");
    const uint32_t width = geometry_.bits == 32 ? 4 : 2;
    const uint32_t offset = geometry_.bits == 12 ? cluster + cluster / 2 : cluster * width;
    const uint32_t address = geometry_.fatStart(geometry_.activeFat) * geometry_.sectorBytes + offset;
    value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t b;
        if (!byte(address + i, b)) return false;
        value |= uint32_t(b) << (8 * i);
    }
    if (geometry_.bits == 12) value = (value >> ((cluster & 1) ? 4 : 0)) & 0xfff;
    if (geometry_.bits == 32) value &= 0x0fffffff;
    return true;
}
bool Volume::setEntry(unsigned copy, uint32_t cluster, uint32_t value) {
    if (copy >= geometry_.copies || cluster >= geometry_.clusters + 2) return fail("INVALID FAT WRITE");
    const unsigned width = geometry_.bits == 32 ? 4 : 2;
    const uint32_t offset = geometry_.bits == 12 ? cluster + cluster / 2 : cluster * width;
    const uint32_t address = geometry_.fatStart(copy) * geometry_.sectorBytes + offset;
    uint32_t old = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t b;
        if (!byte(address + i, b)) return false;
        old |= uint32_t(b) << (8 * i);
    }
    if (geometry_.bits == 12) value = cluster & 1 ? (old & 15) | ((value & 0xfff) << 4) : (old & 0xf000) | (value & 0xfff);
    if (geometry_.bits == 32) value = (old & 0xf0000000) | (value & 0x0fffffff);
    for (unsigned i = 0; i < width; ++i) if (!put(address + i, value >> (8 * i))) return false;
    return true;
}
}
