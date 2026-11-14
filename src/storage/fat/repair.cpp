#include "openosc/storage/fat/checker.hpp"
#include <cstring>

namespace openosc::fat {
bool Checker::repair(uint32_t expectedFingerprint) {
    if (!check()) return false;
    if (report_.blocked()) return fail("AMBIGUOUS DAMAGE. REPAIR ON PC.");
    if (report_.fingerprint != expectedFingerprint) return fail("DISK CHANGED. CHECK AGAIN.");
    if (!report_.fixes()) return true;
    const auto& g = volume_.geometry();
    observer_.progress("REPAIRING DIRECTORY METADATA", 0, 1);
    if (!directory(g.rootCluster, true, 0, true) || !repairAllocation()
        || !repairMetadata() || !volume_.finish()) return false;
    if (!check()) return false;
    return (!report_.blocked() && !report_.fixes()) || fail("FAT ERRORS REMAIN AFTER REPAIR");
}
bool Checker::reconcileMetadata() {
    if (!check()) return true;
    const auto& r = report_;
    if (r.blocked() || r.erasedEntries || r.directorySizes || r.lostClusters || r.info) return true;
    return !(r.dirty || r.label) || repair(r.fingerprint);
}
bool Checker::repairAllocation() {
    const auto& g = volume_.geometry();
    for (unsigned copy = 0; copy < g.copies; ++copy) {
        if (!g.mirrored && copy != g.activeFat) continue;
        for (uint32_t c = 2; c < g.clusters + 2; ++c) {
            if (marked(c) && !volume_.setEntry(copy, c, 0)) return false;
            if (!(c & 255)) observer_.progress("RECLAIMING UNLINKED CLUSTERS", c - 2, g.clusters);
        }
        const uint32_t clean = g.bits == 16 ? 0x8000 : g.bits == 32 ? 0x08000000 : 0;
        if (clean) {
            uint32_t value;
            if (!volume_.entry(1, value) || !volume_.setEntry(copy, 1, value | clean)) return false;
        }
        if (!volume_.flush()) return false;
    }
    return true;
}
bool Checker::repairMetadata() {
    const auto& g = volume_.geometry();
    const unsigned flags = g.bits == 32 ? 65 : 37, label = g.bits == 32 ? 71 : 43;
    for (unsigned backup = 0; backup < 2; ++backup) {
        if (backup && (g.bits != 32 || !g.backupSector || g.backupSector == 0xffff)) continue;
        const uint32_t sector = g.start + (backup ? g.backupSector : 0);
        if (!volume_.read(sector, buffer_)) return false;
        buffer_[flags] &= ~1U;
        const void* value = hasLabel_ ? static_cast<const void*>(label_) : static_cast<const void*>("NO NAME    ");
        if (buffer_[g.bits == 32 ? 66 : 38] == 0x29) std::memcpy(buffer_ + label, value, 11);
        if (!volume_.write(sector, buffer_)) return false;
    }
    if (g.bits == 32 && g.infoSector && g.infoSector != 0xffff) {
        for (unsigned backup = 0; backup < 2; ++backup) {
            if (backup && (!g.backupSector || g.backupSector == 0xffff
                || uint32_t(g.backupSector) + g.infoSector >= g.reserved)) continue;
            const uint32_t sector = g.start + g.infoSector + (backup ? g.backupSector : 0);
            if (!volume_.read(sector, buffer_)) return false;
            Volume::put32(buffer_, 0x41615252); Volume::put32(buffer_ + 484, 0x61417272);
            Volume::put32(buffer_ + 488, report_.freeClusters + report_.lostClusters);
            Volume::put32(buffer_ + 492, 0xffffffff); Volume::put32(buffer_ + 508, 0xaa550000);
            if (!volume_.write(sector, buffer_)) return false;
        }
    }
    return true;
}
}
