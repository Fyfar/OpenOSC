#pragma once
#include "openosc/display/display.hpp"
#include "openosc/storage/fat/checker.hpp"
#include "openosc/storage/local_sectors.hpp"

namespace openosc {
class FatCheckOperation final : private fat::Observer {
public:
    FatCheckOperation(BlockDevice& disk, Display& screen) : disk_(disk), checker_(disk_, *this), screen_(screen) {}
    bool inspect();
    bool repair();
    bool reconcile() { return checker_.reconcileMetadata(); } // Dirty flag and BPB label only; see Checker.
    const fat::Report& report() const { return checker_.report(); }
    const char* error() const { return checker_.error(); }
private:
    void progress(const char* phase, uint32_t done, uint32_t total) override;
    LocalSectors disk_;
    fat::Checker checker_;
    Display& screen_;
    uint32_t fingerprint_ = 0, lastDraw_ = 0;
};
}
