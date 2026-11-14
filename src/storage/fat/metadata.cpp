#include "openosc/storage/fat/checker.hpp"
#include <cstring>

namespace openosc::fat {
bool Checker::metadata() {
    const auto& g = volume_.geometry();
    if (!volume_.read(g.start, buffer_)) return false;
    fingerprint_.update(buffer_, g.sectorBytes);
    report_.dirty = buffer_[g.bits == 32 ? 65 : 37] & 1;
    uint32_t first, second;
    if (!volume_.entry(0, first) || !volume_.entry(1, second)) return false;
    const uint32_t flags = g.bits == 16 ? 0xc000 : g.bits == 32 ? 0x0c000000 : 0;
    if ((first & 255) != g.media || (first | 7U) < g.eoc() || (second | flags) < g.eoc())
        ++report_.invalidEntries;
    if (flags) {
        if (!(second & (flags & (flags << 1)))) report_.dirty = true;
        if (!(second & (flags & (flags >> 1)))) ++report_.invalidEntries;
    }
    uint8_t other[4096];
    for (uint32_t sector = 0; sector < g.fatSectors; ++sector) {
        if (!volume_.read(g.fatStart(g.activeFat) + sector, buffer_)) return false;
        fingerprint_.update(buffer_, g.sectorBytes);
        if (g.mirrored) for (unsigned copy = 1; copy < g.copies; ++copy) {
            if (!volume_.read(g.fatStart(copy) + sector, other)) return false;
            fingerprint_.update(other, g.sectorBytes);
            if (std::memcmp(buffer_, other, g.sectorBytes)) ++report_.mirrorErrors;
        }
        observer_.progress("COMPARING FAT COPIES", sector + 1, g.fatSectors);
    }
    if (g.bits == 32 && g.backupSector && g.backupSector != 0xffff) {
        if (!volume_.read(g.start, buffer_) || !volume_.read(g.start + g.backupSector, other)) return false;
        fingerprint_.update(other, g.sectorBytes);
        if (other[510] != 0x55 || other[511] != 0xaa) ++report_.invalidEntries;
        for (unsigned i = 11; i < 90; ++i) {
            if (i == 65 || (i >= 71 && i < 82)) continue;
            if (buffer_[i] != other[i]) { ++report_.invalidEntries; break; }
        }
        if ((other[65] & 1) || std::memcmp(buffer_ + 71, other + 71, 11)) report_.dirty = true;
    }
    return true;
}
}
