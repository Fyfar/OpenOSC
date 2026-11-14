#pragma once
#include "openosc/board/current.hpp"

namespace openosc::dmm {
// The counts scale the bar and the dial draw: full deflection at the controller's display maximum, so
// no valid reading pegs them (the decoder's 66000 is overload, never a reading). A minor tick every
// 1000 counts, a labelled one every `major` thousand. ZT703S 25000 (0..25 by 5), as before.
// ZT706S 66000 (0..66 by 11): the manual's 66000 counts. The factory 706S dial (08042778) clamps
// at 62000, which would peg valid readings of 62000..65999 before overload; not copied.
struct CountScale {
    int fullScale, major;
    constexpr int ticks() const { return fullScale / 1000; }
    constexpr int labels() const { return ticks() / major + 1; }
    static constexpr CountScale of(board::DmmCounts counts) {
        return counts == board::DmmCounts::Counts66000 ? CountScale{66000, 11} : CountScale{25000, 5};
    }
};
inline constexpr CountScale BoardScale = CountScale::of(board::Current::Features.dmmCounts);
static_assert(BoardScale.fullScale % 1000 == 0 && BoardScale.ticks() % BoardScale.major == 0, "whole labelled ticks");
} // namespace openosc::dmm
