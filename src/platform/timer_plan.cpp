#include "openosc/platform/timer_plan.hpp"
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numeric>

namespace openosc {
bool TimerPlan::calculate(uint32_t clock, Rational requested, TimerPlan &output, uint32_t minimum) {
    if (!clock || !requested.valid() || !minimum || minimum > 65536)
        return false;
    const uint64_t source = uint64_t(clock) * requested.denominator;
    if (uint64_t(requested.numerator) * minimum > source ||
        source > uint64_t(requested.numerator) * 65536 * 65536)
        return false;
    const uint64_t max = uint64_t(requested.numerator) * 65536;
    const uint32_t first = static_cast<uint32_t>(source / max + (source % max != 0));
    double best = std::numeric_limits<double>::infinity();
    TimerPlan candidate;
    for (uint32_t prescaler = first ? first : 1; prescaler <= 65536; ++prescaler) {
        const uint64_t denominator = uint64_t(requested.numerator) * prescaler;
        const uint64_t ideal = source / denominator;
        if (ideal + 1 < minimum)
            break;
        for (uint64_t period : {ideal, ideal + 1}) {
            if (period < minimum || period > 65536)
                continue;
            const uint64_t divisor = uint64_t(prescaler) * period;
            const uint64_t common = std::gcd(uint64_t(clock), divisor);
            if (divisor / common > UINT32_MAX)
                continue;
            const double error = std::fabs(double(clock) / divisor - requested.value());
            if (error >= best)
                continue;
            best = error;
            candidate.prescaler = prescaler - 1;
            candidate.reload = period - 1;
            candidate.actualHz = {uint32_t(clock / common), uint32_t(divisor / common)};
        }
        if (best == 0)
            break;
    }
    if (!std::isfinite(best))
        return false;
    output = candidate;
    return true;
}
} // namespace openosc
