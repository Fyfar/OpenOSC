#pragma once
#include "openosc/storage/sector_device.hpp"

namespace openosc::fat {
struct Geometry {
    uint32_t start = 0, sectors = 0, reserved = 0, fatSectors = 0;
    uint32_t rootSector = 0, rootSectors = 0, rootCluster = 0;
    uint32_t dataSector = 0, clusters = 0;
    uint16_t sectorBytes = 0, rootEntries = 0, infoSector = 0, backupSector = 0;
    uint8_t clusterSectors = 0, copies = 0, activeFat = 0, bits = 0, media = 0;
    bool mirrored = true;
    uint32_t clusterBytes() const { return uint32_t(sectorBytes) * clusterSectors; }
    uint32_t fatStart(unsigned copy) const { return start + reserved + copy * fatSectors; }
    uint32_t clusterSector(uint32_t cluster) const { return dataSector + (cluster - 2) * clusterSectors; }
    uint32_t eoc() const { return bits == 12 ? 0xff8 : bits == 16 ? 0xfff8 : 0x0ffffff8; }
    uint32_t bad() const { return eoc() - 1; }
    bool validCluster(uint32_t c) const { return c >= 2 && c - 2 < clusters; }
};
class Volume {
public:
    explicit Volume(SectorDevice& disk) : disk_(disk) {}
    bool open();
    bool read(uint32_t sector, uint8_t* data);
    bool write(uint32_t sector, const uint8_t* data);
    bool entry(uint32_t cluster, uint32_t& value);
    bool setEntry(unsigned copy, uint32_t cluster, uint32_t value);
    bool flush();
    bool finish();
    const Geometry& geometry() const { return geometry_; }
    const char* error() const { return error_; }
    static uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
    static uint32_t u32(const uint8_t* p) { return u16(p) | (uint32_t(u16(p + 2)) << 16); }
    static void put32(uint8_t* p, uint32_t value);
private:
    bool parse(uint32_t start, uint32_t limit);
    bool load(uint32_t sector);
    bool byte(uint32_t address, uint8_t& value);
    bool put(uint32_t address, uint8_t value);
    bool fail(const char* message);
    SectorDevice& disk_;
    Geometry geometry_{};
    uint32_t cachedSector_ = 0xffffffff;
    bool dirty_ = false;
    const char* error_ = nullptr;
    alignas(4) uint8_t cache_[4096], verify_[4096];
};
}
