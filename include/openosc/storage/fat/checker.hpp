#pragma once
#include "openosc/storage/fat/volume.hpp"
#include "openosc/update/crc32.hpp"

namespace openosc::fat {
struct Report {
    uint32_t files = 0, directories = 0, erasedEntries = 0, directorySizes = 0;
    uint32_t lostClusters = 0, freeClusters = 0, badClusters = 0;
    uint32_t chainErrors = 0, invalidEntries = 0, mirrorErrors = 0;
    uint32_t fingerprint = 0;
    uint8_t bits = 0;
    bool dirty = false, label = false, info = false;
    bool blocked() const { return chainErrors || invalidEntries || mirrorErrors; }
    uint32_t fixes() const { return erasedEntries + directorySizes + lostClusters + dirty + label + info; }
};
class Observer {
public:
    virtual void progress(const char* phase, uint32_t done, uint32_t total) = 0;
protected:
    ~Observer() = default;
};
class Checker {
public:
    static constexpr uint32_t MaxClusters = 262144;
    Checker(SectorDevice& disk, Observer& observer) : volume_(disk), observer_(observer) {}
    bool check();
    bool repair(uint32_t expectedFingerprint);
    // Host FAT drivers leave the dirty flag after a session ended without unmount, and the factory boot
    // renames the volume in the root entry only (f_setlabel), so BPB and root labels disagree. Repairs
    // exactly these two after a full check found nothing else (nothing blocked, no entries, sizes,
    // clusters or FSInfo to fix); otherwise writes nothing and leaves the volume to an explicit repair.
    // A volume that cannot be checked is left to the mount. False only when the repair failed (error()).
    bool reconcileMetadata();
    const Report& report() const { return report_; }
    const char* error() const { return error_ ? error_ : volume_.error(); }
private:
    bool metadata();
    bool allocation();
    bool directory(uint32_t cluster, bool root, unsigned depth, bool repair);
    bool directorySector(uint32_t sector, unsigned entries, bool root, unsigned depth, bool repair, bool& ended);
    bool claim(uint32_t cluster, uint32_t& count);
    bool repairAllocation();
    bool repairMetadata();
    bool fail(const char* message);
    bool marked(uint32_t c) const { return bitmap_[c / 8] & (1U << (c & 7)); }
    void mark(uint32_t c, bool set);
    Volume volume_;
    Observer& observer_;
    Report report_{};
    Crc32 fingerprint_;
    const char* error_ = nullptr;
    bool hasLabel_ = false;
    uint8_t label_[11]{};
    alignas(4) uint8_t buffer_[4096];
    uint8_t bitmap_[(MaxClusters + 2 + 7) / 8];
};
}
