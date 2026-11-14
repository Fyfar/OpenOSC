#include "openosc/storage/fat/checker.hpp"
#include <cstring>

namespace openosc::fat {
bool Checker::fail(const char* message) { error_ = message; return false; }
void Checker::mark(uint32_t cluster, bool set) {
    auto& byte = bitmap_[cluster / 8];
    const uint8_t mask = 1U << (cluster & 7);
    byte = set ? byte | mask : byte & ~mask;
}
bool Checker::check() {
    report_ = {}; error_ = nullptr; hasLabel_ = false; fingerprint_ = Crc32();
    std::memset(bitmap_, 0, sizeof bitmap_);
    if (!volume_.open()) return false;
    const auto& g = volume_.geometry();
    report_.bits = g.bits;
    if (g.clusters > MaxClusters) return fail("FAT CHECK CLUSTER LIMIT EXCEEDED");
    observer_.progress("CHECKING FAT METADATA", 0, 1);
    if (!metadata() || !directory(g.rootCluster, true, 0, false) || !allocation()) return false;
    if (!volume_.read(g.start, buffer_)) return false;
    const unsigned extended = g.bits == 32 ? 66 : 38;
    const unsigned labelOffset = g.bits == 32 ? 71 : 43;
    const uint8_t* expected = hasLabel_ ? label_ : reinterpret_cast<const uint8_t*>("NO NAME    ");
    if (buffer_[extended] == 0x29) {
        bool blank = true;
        for (unsigned i = 0; i < 11; ++i) if (buffer_[labelOffset + i] != ' ') blank = false;
        report_.label = std::memcmp(buffer_ + labelOffset, expected, 11) && (!blank || hasLabel_);
    }
    if (g.bits == 32 && g.infoSector && g.infoSector != 0xffff) {
        for (unsigned backup = 0; backup < 2; ++backup) {
            if (backup && (!g.backupSector || g.backupSector == 0xffff
                || uint32_t(g.backupSector) + g.infoSector >= g.reserved)) continue;
            if (!volume_.read(g.start + g.infoSector + (backup ? g.backupSector : 0), buffer_)) return false;
            fingerprint_.update(buffer_, g.sectorBytes);
            const uint32_t free = Volume::u32(buffer_ + 488), next = Volume::u32(buffer_ + 492);
            report_.info |= Volume::u32(buffer_) != 0x41615252 || Volume::u32(buffer_ + 484) != 0x61417272
                || Volume::u32(buffer_ + 508) != 0xaa550000
                || (free != 0xffffffff && free != report_.freeClusters)
                || (next != 0xffffffff && !g.validCluster(next));
        }
    }
    report_.fingerprint = fingerprint_.value();
    observer_.progress("FAT CHECK COMPLETE", 1, 1);
    return true;
}
bool Checker::claim(uint32_t cluster, uint32_t& count) {
    const auto& g = volume_.geometry();
    count = 0;
    if (!cluster) return true;
    while (true) {
        if (!g.validCluster(cluster) || marked(cluster)) { ++report_.chainErrors; return true; }
        mark(cluster, true); ++count;
        uint32_t next;
        if (!volume_.entry(cluster, next)) return false;
        if (next >= g.eoc()) return true;
        if (next == g.bad() || next < 2 || next >= g.eoc() - 8) { ++report_.chainErrors; return true; }
        cluster = next;
    }
}
bool Checker::allocation() {
    const auto& g = volume_.geometry();
    for (uint32_t c = 2; c < g.clusters + 2; ++c) {
        uint32_t value;
        if (!volume_.entry(c, value)) return false;
        if (!value) ++report_.freeClusters;
        if (value == g.bad()) ++report_.badClusters;
        const bool lost = !marked(c) && value && value != g.bad();
        if (lost) ++report_.lostClusters;
        // After traversal, the same bitmap becomes the exact orphan repair set.
        mark(c, lost);
        if (!(c & 255)) observer_.progress("CHECKING CLUSTER ALLOCATION", c - 2, g.clusters);
    }
    return true;
}
}
