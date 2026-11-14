#include "openosc/acquisition/interleave.hpp"

namespace openosc::scope {
double InterleaveResponse::curve(uint32_t hz) {
    // Nodes as the factory table (08053A20) and gains (1, 1, 1.31, 1, 1.9); the segment is chosen as
    // 0804D720 does: below 40 MHz the first, [40, 46) the second, [46, 60) the third, from 60 MHz the last.
    static constexpr double Hz[] = {30e6, 40e6, 46e6, 60e6, 100e6}, Gain[] = {1.0, 1.0, 1.31, 1.0, 1.9};
    const unsigned right = hz < 40000000 ? 1 : hz < 46000000 ? 2 : hz < 60000000 ? 3 : 4;
    const double slope = (Gain[right] - Gain[right - 1]) / (Hz[right] - Hz[right - 1]);
    return Gain[right] + slope * (double(hz) - Hz[right]);
}
} // namespace openosc::scope
