#include "openosc/ui/scope/controls.hpp"

namespace openosc::plot {
namespace {
constexpr unsigned rows[][14] = {
    {Controls::View, Controls::Channel, Controls::Zoom, Controls::Pan, Controls::Visible,
     Controls::Invert, Controls::Gain, Controls::Position, Controls::GridStyle, Controls::MathTrace},
    {Controls::SampleReduction, Controls::TraceInterpolation, Controls::TraceStyle, Controls::Average,
     Controls::PeakHold, Controls::Glow},
    {Controls::Channel, Controls::TimeCursors, Controls::VoltageCursors, Controls::CursorAX,
     Controls::CursorBX, Controls::CursorAY, Controls::CursorBY},
    {Controls::Persistence, Controls::Reference, Controls::Clear},
    {Controls::FftSource, Controls::FftWindow, Controls::FftScale, Controls::FftDc}
};
constexpr unsigned counts[] = {10, 6, 7, 3, 4};
constexpr const char *names[] = {"View", "Capture rendering", "Cursors", "History", "FFT"};
}
const char *Controls::groupName(unsigned g) { return g < unsigned(Group::Count) ? names[g] : ""; }
const char *Controls::section() const { return groupName(unsigned(group)); }
unsigned Controls::count() const { return counts[unsigned(group)] + 1; }
unsigned Controls::row(unsigned index) const {
    if (!index || index >= count()) return Section;
    return rows[unsigned(group)][index-1];
}
unsigned Controls::index(unsigned selected) const {
    for (unsigned i=0; i<count(); ++i) if (row(i)==selected) return i;
    return 0;
}
void Controls::open(unsigned selected) {
    if (selected==Section) return;
    for (unsigned g=0; g<unsigned(Group::Count); ++g)
        for (unsigned i=0; i<counts[g]; ++i) if (rows[g][i]==selected) { group=Group(g); return; }
}
} // namespace openosc::plot
