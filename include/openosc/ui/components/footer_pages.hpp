#pragma once
#include "openosc/acquisition/readouts.hpp"
#include "openosc/ui/components/scope_footer_layout.hpp"
#include <cstdint>

namespace openosc::ui {
// Which footer page shows what (no LVGL: the plan alone). The selected readouts of each channel,
// Off slots skipped, go three per page into the channel's panel, so the first page holds the
// first three of both channels (f, Vpp and RMS by default) and REL brings the fourth; a channel
// that is off shows "CHn Off" there instead. A Scales page (both channels' coupling, probe and V/div) follows while the
// display shows scales, and FFT puts its spectrum page (peak, Hz/div) first. REL turns the pages; dots show how
// many there are.
class FooterPages {
public:
    enum class Kind : uint8_t { Spectrum, Readouts, Scales };
    static constexpr uint8_t Empty = 0xff, ChannelOff = 0xfe; // Cell contents besides a slot index.
    struct Cell { uint8_t channel = 0, slot = Empty; };
    static constexpr unsigned Cells = ScopeFooterLayout::Cells, PerChannel = ScopeFooterLayout::PerChannel;
    static constexpr unsigned MaximumPages = ScopeFooterLayout::MaximumPages;

    // True when the plan changed (the page on show is then kept only if it still exists).
    bool plan(const scope::ReadoutSelection &selection, const bool (&enabled)[2], bool spectrum, bool scales);
    unsigned count() const { return count_; }
    unsigned page() const { return page_; }
    void next() { page_ = count_ ? (page_ + 1) % count_ : 0; }
    // Only valid while count() is not zero.
    Kind kind() const { return kind(page_); }
    Kind kind(unsigned page) const;
    // A readout page's cell: the channel's slot, ChannelOff or Empty.
    Cell cell(unsigned index) const { return cell(page_, index); }
    Cell cell(unsigned page, unsigned index) const;

private:
    uint8_t slots_[2][scope::ReadoutSelection::PerChannel]{}, active_[2]{};
    bool enabled_[2]{}, spectrum_ = false, scales_ = false;
    unsigned readoutPages_ = 0, count_ = 0, page_ = 0;
};
} // namespace openosc::ui
