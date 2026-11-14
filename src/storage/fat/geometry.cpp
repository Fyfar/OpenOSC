#include "openosc/storage/fat/volume.hpp"

namespace openosc::fat {
bool Volume::parse(uint32_t start, uint32_t limit) {
    if (!read(start, cache_)) return false;
    const auto* p = cache_;
    if (p[510] != 0x55 || p[511] != 0xaa) return false;
    Geometry g;
    g.start = start; g.sectorBytes = u16(p + 11); g.clusterSectors = p[13];
    g.reserved = u16(p + 14); g.copies = p[16]; g.rootEntries = u16(p + 17);
    g.sectors = u16(p + 19) ? u16(p + 19) : u32(p + 32);
    g.fatSectors = u16(p + 22) ? u16(p + 22) : u32(p + 36);
    g.media = p[21];
    if (g.sectorBytes != disk_.sectorSize() || !g.clusterSectors
        || (g.clusterSectors & (g.clusterSectors - 1)) || !g.reserved
        || !g.copies || g.copies > 4 || !g.fatSectors || g.sectors > limit) return false;
    const uint64_t root = uint64_t(start) + g.reserved + uint64_t(g.copies) * g.fatSectors;
    g.rootSectors = (uint32_t(g.rootEntries) * 32 + g.sectorBytes - 1) / g.sectorBytes;
    const uint64_t firstData = root + g.rootSectors;
    if (firstData >= uint64_t(start) + g.sectors || uint64_t(start) + g.sectors > disk_.sectorCount()) return false;
    g.rootSector = root; g.dataSector = firstData;
    g.clusters = (start + g.sectors - g.dataSector) / g.clusterSectors;
    g.bits = g.clusters < 4085 ? 12 : g.clusters < 65525 ? 16 : 32;
    if (uint64_t(g.clusters + 2) * g.bits > uint64_t(g.fatSectors) * g.sectorBytes * 8) return false;
    if (g.bits == 32) {
        if (g.rootEntries || u16(p + 22) || u16(p + 42)) return false;
        const uint16_t flags = u16(p + 40);
        g.mirrored = !(flags & 0x80);
        g.activeFat = g.mirrored ? 0 : flags & 15;
        g.rootCluster = u32(p + 44);
        g.infoSector = u16(p + 48); g.backupSector = u16(p + 50);
        if (g.activeFat >= g.copies || !g.validCluster(g.rootCluster)) return false;
        if (g.infoSector != 0xffff && g.infoSector && g.infoSector >= g.reserved) return false;
        if (g.backupSector != 0xffff && g.backupSector && g.backupSector >= g.reserved) return false;
    } else if (!g.rootEntries || !u16(p + 22)) return false;
    geometry_ = g;
    return true;
}
bool Volume::open() {
    error_ = nullptr; dirty_ = false; cachedSector_ = 0xffffffff;
    uint32_t size = disk_.sectorSize();
    if (size < 512 || size > 4096 || (size & (size - 1))) return fail("UNSUPPORTED SECTOR SIZE");
    if (uint64_t(size) * disk_.sectorCount() > 0xffffffffULL) return fail("FAT DISK TOO LARGE");
    if (parse(0, disk_.sectorCount())) return true;
    if (error_) return false;
    if (!read(0, verify_) || verify_[510] != 0x55 || verify_[511] != 0xaa)
        return fail("NO VALID FAT BOOT SECTOR");
    uint32_t starts[4], limits[4];
    for (unsigned i = 0; i < 4; ++i) {
        starts[i] = u32(verify_ + 446 + 16 * i + 8);
        limits[i] = u32(verify_ + 446 + 16 * i + 12);
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (starts[i] && starts[i] < disk_.sectorCount() && limits[i]
            && limits[i] <= disk_.sectorCount() - starts[i] && parse(starts[i], limits[i])) return true;
        if (error_) return false;
    }
    return fail("NO SUPPORTED FAT PARTITION");
}
}
