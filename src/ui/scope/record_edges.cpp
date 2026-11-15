#include "openosc/ui/scope/painter.hpp"
#include "openosc/ui/components/elements.hpp"
#include <algorithm>

namespace openosc::plot {
namespace {
constexpr uint8_t TintOpacity = LV_OPA_30, HatchOpacity = LV_OPA_50, EdgeOpacity = LV_OPA_70;
constexpr int HatchStep = 12; // Pixels between the hatch's diagonals.
constexpr int HatchMinimum = 3 * HatchStep; // A narrower part keeps the tint and the edge only.
constexpr int TextHeight = 14; // Painter::text's box.
} // namespace
// Under the grid: each part of the window outside the record gets a faint tint of the grid's
// colour, sparse diagonals where it is wide and a dashed line on its last column next to the
// record. Nothing is drawn for a window inside the record.
// Invariant: draw() calls this right after its opaque clear of the whole frame (the background)
// and before the grid and traces, under the same clip, also for a dirty area. Every pixel under
// the tint is therefore the background, so the 30 % blend is computed once and filled opaquely
// (no blending reads of the frame).
void Painter::recordGaps(const RecordGaps &gaps) {
    if (!gaps.any()) return;
    const int h = lv_area_get_height(&area_);
    const lv_area_t saved = layer_->_clip_area;
    const uint32_t tint = lv_color_to_int(lv_color_mix(lv_color_hex(palette_.grid), lv_color_hex(palette_.background), TintOpacity));
    const auto region = [&](int x0, int x1, int edge) { // Plot columns [x0, x1); `edge`: the dashed column, or -1.
        const lv_area_t box{area_.x1 + x0, area_.y1, area_.x1 + x1 - 1, area_.y2};
        const lv_area_t clip{std::max(box.x1, saved.x1), std::max(box.y1, saved.y1), std::min(box.x2, saved.x2), std::min(box.y2, saved.y2)};
        if (clip.x1 > clip.x2 || clip.y1 > clip.y2) return; // This part is not in the dirty area.
        layer_->_clip_area = clip;
        fill(box, tint);
        if (x1 - x0 >= HatchMinimum) {
            Path hatch(layer_, area_, palette_.grid, HatchOpacity, 1, model_.statistics());
            // Diagonals (k + t, h - 1 - t), k a multiple of HatchStep: fixed to the plot's columns,
            // so they stay put while panning. Each is cut to the gap's columns before it becomes a
            // vertex (slope 1, integer: same pixels, same phase), so its task covers only the gap.
            int k = x0 - h + 1;
            k -= ((k % HatchStep) + HatchStep) % HatchStep;
            for (; k < x1; k += HatchStep) {
                const int from = std::max(0, x0 - k), to = std::min(h - 1, x1 - 1 - k);
                if (from > to) continue;
                hatch.vertex(k + from, h - 1 - from);
                hatch.vertex(k + to, h - 1 - to);
                hatch.gap();
            }
        }
        // Path's destructor submits its final batch while the gap's clip is still active.
        if (edge >= 0) line(edge, 0, edge, h - 1, palette_.cursor, EdgeOpacity, true);
        layer_->_clip_area = saved;
    };
    const int w = int(gaps.width);
    if (gaps.before) region(0, int(gaps.before), gaps.whole() ? -1 : int(gaps.before) - 1);
    if (gaps.after) region(w - int(gaps.after), w, gaps.whole() ? -1 : w - int(gaps.after));
}
// Over the traces: what the part is, centred in it, only where the words fit inside it.
void Painter::recordGapLabels(const RecordGaps &gaps) {
    if (!gaps.any() || lv_area_get_height(&area_) < TextHeight) return;
    const auto label = [&](int x0, int x1, const char *full, const char *brief) {
        for (const char *value : {full, brief}) {
            const int width = ui::Elements::textWidth(value) + 4;
            if (width > x1 - x0) continue;
            text(value, x0 + (x1 - x0 - width) / 2, lv_area_get_height(&area_) / 2 - 6, palette_.readout, width, true);
            return;
        }
    };
    const int w = int(gaps.width);
    if (gaps.whole()) label(0, w, "OUTSIDE RECORD", "OUTSIDE");
    else {
        if (gaps.before) label(0, int(gaps.before), "START OF RECORD", "START");
        if (gaps.after) label(w - int(gaps.after), w, "END OF RECORD", "END");
    }
}
} // namespace openosc::plot
