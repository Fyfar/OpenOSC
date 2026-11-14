#include "openosc/storage/fat/checker.hpp"
#include <cstring>

namespace openosc::fat {
bool Checker::directorySector(uint32_t sector, unsigned entries, bool root, unsigned depth, bool repair, bool& ended) {
    const auto& g = volume_.geometry();
    if (!volume_.read(sector, buffer_)) return false;
    if (!repair) fingerprint_.update(buffer_, g.sectorBytes);
    bool changed = false;
    for (unsigned i = 0; i < entries; ++i) {
        uint8_t entry[32];
        std::memcpy(entry, buffer_ + i * 32, sizeof entry);
        bool erased = true;
        for (uint8_t value : entry) if (value != 0xff) erased = false;
        if (erased) {
            if (!repair) ++report_.erasedEntries;
            else { std::memset(buffer_ + i * 32, 0, 32); buffer_[i * 32] = 0xe5; changed = true; }
            continue;
        }
        if (!entry[0]) ended = true;
        if (ended || entry[0] == 0xe5 || entry[11] == 0x0f) continue;
        const uint8_t attr = entry[11];
        const uint32_t cluster = Volume::u16(entry + 26)
            | (g.bits == 32 ? uint32_t(Volume::u16(entry + 20)) << 16 : 0);
        const uint32_t size = Volume::u32(entry + 28);
        if ((attr & 0xc0) || (attr & 0x18) == 0x18) { if (!repair) ++report_.invalidEntries; continue; }
        if (attr & 8) {
            if (!root || cluster || size) { if (!repair) ++report_.invalidEntries; continue; }
            if (!repair) {
                if (hasLabel_ && std::memcmp(label_, entry, 11)) ++report_.invalidEntries;
                std::memcpy(label_, entry, 11); hasLabel_ = true;
            }
            continue;
        }
        bool dot = entry[0] == '.';
        for (unsigned j = entry[1] == '.' ? 2 : 1; j < 11; ++j) dot &= entry[j] == ' ';
        if (entry[0] == '.' && (!dot || !(attr & 16) || root)) {
            if (!repair) ++report_.invalidEntries;
            continue;
        }
        if (attr & 16) {
            if (size) {
                if (!repair) ++report_.directorySizes;
                else { Volume::put32(buffer_ + i * 32 + 28, 0); changed = true; }
            }
            if (dot) continue;
            if (!g.validCluster(cluster)) { if (!repair) ++report_.chainErrors; continue; }
            if (changed && !volume_.write(sector, buffer_)) return false;
            changed = false;
            if (!directory(cluster, false, depth + 1, repair) || !volume_.read(sector, buffer_)) return false;
        } else if (!repair) {
            ++report_.files;
            uint32_t count;
            if (!claim(cluster, count)) return false;
            const uint32_t expected = (uint64_t(size) + g.clusterBytes() - 1) / g.clusterBytes();
            if (count != expected) ++report_.chainErrors;
        }
    }
    return !changed || volume_.write(sector, buffer_);
}
bool Checker::directory(uint32_t cluster, bool root, unsigned depth, bool repair) {
    const auto& g = volume_.geometry();
    if (depth > 24) return fail("DIRECTORY DEPTH LIMIT EXCEEDED");
    uint32_t count = 0;
    if (!repair) {
        ++report_.directories;
        const uint32_t errors = report_.chainErrors;
        if ((g.bits == 32 || !root) && !claim(cluster, count)) return false;
        if (errors != report_.chainErrors) return true;
    }
    bool ended = false;
    if (root && g.bits != 32) {
        for (unsigned s = 0; s < g.rootSectors; ++s) {
            unsigned entries = g.rootEntries - s * (g.sectorBytes / 32);
            if (entries > g.sectorBytes / 32) entries = g.sectorBytes / 32;
            if (!directorySector(g.rootSector + s, entries, true, depth, repair, ended)) return false;
        }
        return true;
    }
    for (uint32_t steps = 0; steps < g.clusters && g.validCluster(cluster); ++steps) {
        for (unsigned s = 0; s < g.clusterSectors; ++s)
            if (!directorySector(g.clusterSector(cluster) + s, g.sectorBytes / 32, root, depth, repair, ended)) return false;
        if (!volume_.entry(cluster, cluster)) return false;
        if (cluster >= g.eoc()) return true;
    }
    return fail("INVALID DIRECTORY CHAIN");
}
}
