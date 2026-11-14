#include "openosc/measurement/dmm/session.hpp"
#include <cmath>
#include <limits>

namespace openosc::dmm {
bool Statistics::push(const NumericValue &value) {
    if (!value.valid() || !std::isfinite(value.si))
        return false;
    if (quantity_ != value.quantity)
        reset();
    if (count_ == std::numeric_limits<uint64_t>::max())
        return false;
    const double adjusted = value.si - correction_, next = sum_ + adjusted;
    if (!std::isfinite(adjusted) || !std::isfinite(next))
        return false;
    quantity_ = value.quantity;
    if (!count_)
        min_ = max_ = value.si;
    if (value.si < min_)
        min_ = value.si;
    if (value.si > max_)
        max_ = value.si;
    correction_ = (next - sum_) - adjusted;
    sum_ = next;
    ++count_;
    return true;
}
StatisticsView StatisticsView::of(const Statistics &s, Quantity shown, double offset) {
    StatisticsView view;
    if (!s.count() || s.quantity() != shown) return view;
    view.valid = true; view.count = s.count(); view.quantity = s.quantity();
    view.minimum = s.minimum() - offset; view.maximum = s.maximum() - offset; view.average = s.average() - offset;
    return view;
}
} // namespace openosc::dmm
