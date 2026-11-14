#include "openosc/ui/scope/record_gaps.hpp"

namespace openosc::plot {
namespace {
enum class Side { Before, Inside, After };
// Where column x lies against a record of `samples` points, by the projection's own rules.
Side side(const Viewport &v, bool aggregate, uint32_t samples, unsigned x) {
    if (aggregate) {
        uint32_t first, last;
        v.column(x, samples, first, last);
        return first < last ? Side::Inside : last == 0 ? Side::Before : Side::After;
    }
    const int64_t position = v.position(x);
    if (position < 0) return Side::Before;
    return uint64_t(position) > (uint64_t(samples - 1) << 16) ? Side::After : Side::Inside;
}
} // namespace
RecordGaps RecordGaps::of(const Viewport &v, uint32_t samples) {
    RecordGaps gaps;
    if (v.width < 2 || !samples) return gaps;
    gaps.width = v.width;
    const bool aggregate = v.aggregates();
    // Monotonic in x: a full window costs one test at each end.
    while (gaps.before < v.width && side(v, aggregate, samples, gaps.before) == Side::Before) ++gaps.before;
    while (gaps.before + gaps.after < v.width && side(v, aggregate, samples, v.width - 1 - gaps.after) == Side::After)
        ++gaps.after;
    return gaps;
}
} // namespace openosc::plot
