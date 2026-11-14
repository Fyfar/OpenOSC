#pragma once
#include "openosc/acquisition/frame.hpp"
#include "openosc/ui/scope/options.hpp"
#include "openosc/ui/scope/viewport.hpp"

namespace openosc::plot {
// The next frame's column reductions, done ahead while the display waits on DMA2D (RenderIdle,
// no LVGL): the plain two-channel Y-T case only (both channels shown, one record of pairs, no
// math, an aggregating window). It reduces by the same function projectTime() uses, so a column
// taken from here is the one it would compute; the rest (other modes, FFT, XY, math, history)
// is projected as before. Keyed by the frame (sequence, timestamp, its trigger annotation) and
// the window, never by anything the draw tasks read.
class ColumnCache {
public:
    static constexpr uint32_t PointsPerStep = 2048; // Record points per idle call (at least one column): fits short DMA2D waits.
    // Reduce further columns of `frame` (until `points` were read): true when all are ready.
    bool prepare(const scope::FrameView &frame, const Options &options, unsigned width, unsigned height,
                 uint32_t points = PointsPerStep);
    // Leading columns ready for this frame and window (0 for any other).
    unsigned ready(const scope::FrameMetadata &frame, const Viewport &viewport) const {
        return same(frame, viewport) ? done_ : 0;
    }
    const scope::ColumnStats (&columns(unsigned x) const)[2] { return stats_[x]; }
    void clear() { valid_ = false; done_ = 0; }
    static bool applies(const scope::FrameView &frame, const Options &options, const Viewport &viewport);
    static bool reduce(const scope::FrameView &frame, uint32_t first, uint32_t last, scope::ColumnStats (&out)[2]);

private:
    bool same(const scope::FrameMetadata &m, const Viewport &v) const {
        return valid_ && m.sequence == sequence_ && m.timestampUs == timestamp_ && m.triggerIndex == trigger_ && v == viewport_;
    }
    uint32_t sequence_ = 0, trigger_ = 0, done_ = 0;
    uint64_t timestamp_ = 0;
    Viewport viewport_{};
    bool valid_ = false;
    scope::ColumnStats stats_[MaximumWidth][2];
};
} // namespace openosc::plot
