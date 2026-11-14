#include "openosc/ui/scope/plot_frame.hpp"
#include <algorithm>
#include <cstdlib>

namespace openosc::plot {
void ZeroTags::place(const int (&zero)[2], const bool (&shown)[2], int plotHeight, int (&centre)[2]) {
    const int half = PlotFrame::TagHeight / 2, spacing = PlotFrame::TagHeight + PlotFrame::TagGap;
    const int top = -PlotFrame::Top + half, bottom = plotHeight - 1 + PlotFrame::Bottom - half;
    for (unsigned c = 0; c < 2; ++c)
        centre[c] = zero[c] < 0 ? top + PlotFrame::ArrowHeight : zero[c] >= plotHeight ? bottom - PlotFrame::ArrowHeight
                                : std::clamp(zero[c], top, bottom);
    if (!shown[0] || !shown[1] || std::abs(centre[0] - centre[1]) >= spacing) return;
    // Upper and lower by level (CH1 above on a tie), a tag apart around their middle, inside the margin.
    const unsigned upper = centre[1] < centre[0] ? 1 : 0, lower = upper ^ 1;
    // An arrow beyond a tag keeps its room at that end.
    const int low = top + (zero[upper] < 0 ? PlotFrame::ArrowHeight : 0);
    const int high = bottom - (zero[lower] >= plotHeight ? PlotFrame::ArrowHeight : 0);
    int first = (centre[0] + centre[1]) / 2 - spacing / 2;
    first = std::clamp(first, low, high - spacing);
    centre[upper] = first; centre[lower] = first + spacing;
}
} // namespace openosc::plot
