#include "openosc/decode/acceleration/plateau.hpp"
#include "openosc/decode/simd.hpp"
#include <cstring>

namespace openosc::decoding::acceleration {
namespace {
using scope::Layout;
// Failing bits of four bytes: the bit 7 of a (possible) AdcBusGlitch suspect byte, all bits of a byte on
// the wrong side of the bound (low level: code > bound; high level: code < bound). Callers OR them over
// a lane and test once against the channel's byte mask.
template <bool High> __attribute__((always_inline)) inline uint32_t failing(uint32_t bytes, uint32_t bound) {
    const uint32_t ok = High ? Simd::atLeast(bytes, bound) : Simd::atLeast(bound, bytes);
    return Simd::suspects(bytes) | ~ok;
}
template <bool High> __attribute__((always_inline)) inline bool byteOk(uint8_t raw, int8_t bound) {
    if (scope::AdcBusGlitch::suspect(raw)) return false;
    return High ? int8_t(raw) >= bound : int8_t(raw) <= bound;
}
// The points of lane `lane` (positions p with p & 3 == lane) in [p0, p0 + n): their first lane offset
// and how many; false when there are none.
__attribute__((always_inline)) inline bool laneSpan(uint32_t p0, uint32_t n, unsigned lane, uint32_t &offset, uint32_t &count) {
    const uint32_t p = p0 + ((lane - p0) & 3u);
    if (p >= p0 + n) return false;
    offset = p >> 2; count = (p0 + n - p + 3) >> 2;
    return true;
}
__attribute__((always_inline)) inline uint32_t load(const void *p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
// 16-bit words of one bank, the channel's byte of each: two words a compare (four an iteration), an odd
// tail alone. Flattened: under -Os the compiler would otherwise call Simd::suspects per word.
template <bool High>
__attribute__((flatten)) bool words(const uint16_t *bank, uint32_t offset, uint32_t count, unsigned channel, uint32_t bound) {
    const uint16_t *w = bank + offset;
    const uint32_t mask = channel ? 0xFF00FF00u : 0x00FF00FFu;
    uint32_t bad = 0;
    for (; count >= 4; count -= 4, w += 4) bad |= failing<High>(load(w), bound) | failing<High>(load(w + 2), bound);
    if (count >= 2) { bad |= failing<High>(load(w), bound); count -= 2; w += 2; }
    if (bad & mask) return false;
    return !count || byteOk<High>(uint8_t(*w >> (8 * channel)), int8_t(bound));
}
// Bytes of one FourByteBanks lane: four a compare (eight an iteration), a tail of 1..3 alone.
template <bool High> __attribute__((flatten)) bool bytes(const uint8_t *lane, uint32_t offset, uint32_t count, uint32_t bound) {
    const uint8_t *b = lane + offset;
    uint32_t bad = 0;
    for (; count >= 8; count -= 8, b += 8) bad |= failing<High>(load(b), bound) | failing<High>(load(b + 4), bound);
    if (count >= 4) { bad |= failing<High>(load(b), bound); count -= 4; b += 4; }
    if (bad) return false;
    for (; count; --count, ++b) if (!byteOk<High>(*b, int8_t(bound))) return false;
    return true;
}
// Split FourBanks: the lane's words through SplitHistory::word (the record's own mapping), gathered in pairs.
template <bool High>
__attribute__((flatten)) bool gathered(const scope::FrameView &frame, unsigned lane, uint32_t offset, uint32_t count, unsigned channel,
                                       uint32_t bound) {
    const auto &split = frame.split();
    const uint16_t *const ahb[4] = {frame.bank(0), frame.bank(1), frame.bank(2), frame.bank(3)};
    const uint32_t laneRing = frame.metadata().ring / 4, mask = channel ? 0xFF00FF00u : 0x00FF00FFu;
    uint32_t bad = 0;
    for (; count >= 2; count -= 2, offset += 2)
        bad |= failing<High>(split.word(ahb, lane, offset, laneRing) | uint32_t(split.word(ahb, lane, offset + 1, laneRing)) << 16, bound);
    if (bad & mask) return false;
    return !count || byteOk<High>(uint8_t(split.word(ahb, lane, offset, laneRing) >> (8 * channel)), int8_t(bound));
}
// Ring positions [p0, p0 + n), p0 + n <= ring: in any order (the level test does not depend on it).
template <bool High>
bool range(const scope::FrameView &frame, PlateauScan::Path path, uint32_t p0, uint32_t n, unsigned channel, uint32_t bound) {
    const Layout layout = frame.metadata().layout;
    if (layout == Layout::Linear) return words<High>(frame.bank(0), p0, n, channel, bound);
    for (unsigned lane = 0; lane < 4; ++lane) {
        uint32_t offset, count;
        if (!laneSpan(p0, n, lane, offset, count)) continue;
        bool ok;
        if (path == PlateauScan::Path::Gather) ok = gathered<High>(frame, lane, offset, count, channel, bound);
        else if (layout == Layout::FourByteBanks) ok = bytes<High>(reinterpret_cast<const uint8_t *>(frame.bank(lane)), offset, count, bound);
        else ok = words<High>(frame.bank(lane), offset, count, channel, bound);
        if (!ok) return false;
    }
    return true;
}
} // namespace

PlateauScan::Path PlateauScan::path(const scope::FrameView &frame, scope::Channel channel) {
    const auto &m = frame.metadata();
    const unsigned c = unsigned(channel);
    if (!frame.valid() || c > 1 || !m.channels[c].enabled) return Path::None;
    switch (m.layout) {
    case Layout::Linear:
    case Layout::FourByteBanks: return frame.split().active() ? Path::None : Path::Simd; // Never split (validate).
    case Layout::FourBanks: return frame.split().active() ? Path::Gather : Path::Simd;
    default: return Path::None; // InterleavedCh1: the HIGH ADC's correction is the scalar path's.
    }
}
bool PlateauScan::quiet(const scope::FrameView &frame, scope::Channel channel, uint32_t first, uint32_t count, int16_t low,
                        int16_t high, bool levelHigh) {
    const Path how = path(frame, channel);
    const auto &m = frame.metadata();
    if (how == Path::None || !count || count > MaxCount || first >= m.samples || count > m.samples - first) return false;
    // The bound in int8: a level no code can move needs only the suspect test; one every code moves fails.
    int32_t bound = levelHigh ? int32_t(low) + 1 : int32_t(high) - 1;
    if (levelHigh ? bound > 127 : bound < -128) return false;
    bound = bound < -128 ? -128 : bound > 127 ? 127 : bound;
    const uint32_t replicated = Simd::replicate(int8_t(bound));
    // The record's points in ring order: (uint32)(origin + index) % ring, the uint32 sum wrapping before
    // the reduction (FrameView's rule). Contiguous ring ranges end at the ring's end and at that wrap.
    uint32_t logical = uint32_t(m.origin + first), remaining = count;
    while (remaining) {
        const uint32_t position = logical % m.ring;
        uint32_t n = m.ring - position < remaining ? m.ring - position : remaining;
        const uint64_t toWrap = (uint64_t(1) << 32) - logical;
        if (toWrap < n) n = uint32_t(toWrap);
        const bool ok = levelHigh ? range<true>(frame, how, position, n, unsigned(channel), replicated)
                                  : range<false>(frame, how, position, n, unsigned(channel), replicated);
        if (!ok) return false;
        logical += n; remaining -= n;
    }
    return true;
}
} // namespace openosc::decoding::acceleration
