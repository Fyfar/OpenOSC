#include "openosc/acquisition/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace openosc::scope {
namespace {
// Keeps values[0..kept) the `kept` first in `before` order of all offered: an insertion into at
// most Outliers + 1 places (each unit of every projection passes here).
template <class Before> void keep(int16_t (&values)[BandEstimate::Outliers + 1], unsigned kept, int16_t value, Before before) {
    unsigned at = kept < BandEstimate::Outliers + 1 ? kept : kept - 1;
    if (at == kept - 1 && !before(value, values[at])) return; // Full, and not among the kept.
    for (; at && before(value, values[at - 1]); --at) values[at] = values[at - 1];
    values[at] = value;
}
} // namespace
void BandEstimate::add(int16_t low, int16_t high) {
    if (!units_++) { low_ = low; high_ = high; }
    low_ = std::min(low_, low); high_ = std::max(high_, high);
    keep(lows_, kept_, low, [](int16_t a, int16_t b) { return a < b; });
    keep(highs_, kept_, high, [](int16_t a, int16_t b) { return a > b; });
    if (kept_ < Outliers + 1) ++kept_;
}
SignalBand BandEstimate::band() const {
    SignalBand b;
    if (!units_) return b;
    b.valid = true;
    if (kept_ <= Outliers) { b.low = low_; b.high = high_; return b; } // Too few units to leave any out.
    b.low = lows_[Outliers]; b.high = highs_[Outliers];
    if (b.high - b.low < 4 && high_ - low_ >= 4) { b.low = low_; b.high = high_; } // Rare pulses: the extremes are the signal.
    return b;
}
namespace {
struct Plateau { int level, margin; bool found; };
// One side's plateau over codes [from, to] (to < from: none), the outermost window on ties:
// scanned from the band edge inward, a later window must hold more.
Plateau plateau(const uint16_t (&counts)[256], int from, int to, int w, uint32_t centre, bool upward) {
    const auto count = [&](int code) { return uint32_t(counts[code + 128]); };
    uint32_t side = 0;
    for (int c = from; c <= to; ++c) side += count(c);
    uint32_t best = 0; int bestAt = 0;
    for (int k = 0; k <= to - from; ++k) {
        const int m = upward ? from + k : to - k;
        uint32_t n = 0;
        for (int c = std::max(from, m - w); c <= std::min(to, m + w); ++c) n += count(c);
        if (n > best) { best = n; bestAt = m; }
    }
    if (best < StateLevels::MinimumCodes || 3 * best < side || best < 2 * centre) return {0, 0, false};
    int64_t sum = 0, squares = 0;
    for (int c = std::max(from, bestAt - w); c <= std::min(to, bestAt + w); ++c) { sum += int64_t(c) * count(c); squares += int64_t(c) * c * count(c); }
    const double mean = double(sum) / best, variance = double(squares) / best - mean * mean;
    return {int(std::lround(mean)), int(std::lround(StateLevels::NoiseMargin * std::sqrt(std::max(0.0, variance)))), true};
}
} // namespace
void StateLevels::addColumn(const FrameView &frame, Channel channel, uint32_t column, uint32_t from, uint32_t to) {
    const uint32_t start = column * Column, offset = column * Stride % Column;
    for (const uint32_t at : {start + offset, start + (offset + Column / 2) % Column}) {
        int16_t code;
        if (at >= from && at < to && frame.signedCode(channel, at, code)) add(code);
    }
}
SignalBand StateLevels::thresholds(const SignalBand &band) const {
    const int low = band.low, high = band.high, span = high - low, twice = low + high;
    if (!band.valid || span < 4) return band;
    const int w = std::max(1, span / 16);
    uint32_t centre = 0; // Codes within w of the middle: |2c - T| <= 2w.
    for (int c = low; c <= high; ++c) if (std::abs(2 * c - twice) <= 2 * w) centre += counts_[c + 128];
    // Low side: 2c < T, the codes [low, ceil(T/2) - 1]; high side: 2c > T, [floor(T/2) + 1, high].
    const int lowTop = (twice + 1 + 256) / 2 - 128 - 1, highBottom = (twice + 256) / 2 - 128 + 1;
    const Plateau a = plateau(counts_, low, lowTop, w, centre, true), b = plateau(counts_, highBottom, high, w, centre, false);
    SignalBand levels = band;
    if (a.found) levels.low = int16_t(std::max(low, a.level - a.margin));
    if (b.found) levels.high = int16_t(std::min(high, b.level + b.margin));
    return levels.high - levels.low < 4 ? band : levels;
}
bool SignalBands::measure(const FrameView &frame, SignalBand (&bands)[2], StateLevels (*levels)[2]) {
    bands[0] = bands[1] = {};
    if (!frame.valid()) return false;
    const uint32_t samples = frame.metadata().samples;
    BandEstimate estimates[2];
    if (frame.pairs()) { // Both channels in one pass of the pair reducer.
        auto columns = frame.pairCursor(0);
        for (uint32_t at = 0; at < samples;) {
            ColumnStats stats[2];
            const uint32_t n = columns.column(std::min(Column, samples - at), stats);
            if (!n) break;
            for (unsigned c = 0; c < 2; ++c) {
                estimates[c].add(stats[c].low, stats[c].high);
                if (levels) (*levels)[c].addColumn(frame, Channel(c), at / Column, at, at + n);
            }
            at += n;
        }
    } else {
        for (unsigned c = 0; c < 2; ++c) {
            if (!frame.metadata().channels[c].enabled) continue;
            auto cursor = frame.cursor(Channel(c));
            for (uint32_t at = 0; at < samples;) {
                ColumnStats stats;
                const uint32_t n = cursor.column(std::min(Column, samples - at), stats);
                if (!n) break;
                estimates[c].add(stats.low, stats.high);
                if (levels) (*levels)[c].addColumn(frame, Channel(c), at / Column, at, at + n);
                at += n;
            }
        }
    }
    for (unsigned c = 0; c < 2; ++c) bands[c] = estimates[c].band();
    return bands[0].valid || bands[1].valid;
}
} // namespace openosc::scope
