#include "openosc/acquisition/record_moments.hpp"
#include "openosc/acquisition/adc_bus_glitch.hpp"
#include <algorithm>

namespace openosc::scope {
namespace {
class Walker {
public:
    Walker(const FrameView &frame, unsigned channel, uint32_t first, dsp::Moments &out)
        : frame_(frame), m_(frame.metadata()), channel_(channel), first_(first),
          start_(uint32_t(m_.origin + first) % m_.ring), out_(out) {}
    // Positions [a, b) of the ring, a <= b <= ring.
    void positions(uint32_t a, uint32_t b) {
        if (m_.layout == Layout::Linear) { words(frame_.bank(0) + a, b - a, a, 1); return; }
        for (unsigned bank = 0; bank < 4; ++bank) {
            const uint32_t o0 = a > bank ? (a - bank + 3) / 4 : 0, o1 = b > bank ? (b - bank + 3) / 4 : 0;
            if (o0 >= o1) continue;
            if (m_.layout == Layout::FourByteBanks)
                bytes(reinterpret_cast<const uint8_t *>(frame_.bank(bank)) + o0, o1 - o0, 4 * o0 + bank);
            else if (!frame_.split().active())
                words(frame_.bank(bank) + o0, o1 - o0, 4 * o0 + bank, 4);
            else
                splitLane(bank, o0, o1);
        }
    }

private:
    // A split lane (SplitHistory::word): lane offsets in runs that are contiguous in the capture ring
    // or in the history; age falls by one per offset.
    void splitLane(unsigned lane, uint32_t o, uint32_t end) {
        const auto &s = frame_.split();
        const uint32_t laneRing = m_.ring / 4;
        while (o < end) {
            uint32_t age = s.end[lane] + laneRing - 1 - o;
            if (age >= laneRing) age -= laneRing;
            const uint16_t *run;
            uint32_t length;
            if (age < s.ahbWords) {
                const uint32_t at = o & (s.ahbWords - 1);
                run = frame_.bank(lane) + at; length = std::min(age + 1, s.ahbWords - at);
            } else {
                uint32_t index = s.newest[lane] + 2 * s.historyWords - age;
                while (index >= s.historyWords) index -= s.historyWords;
                run = s.older[lane] + index; length = std::min(age - s.ahbWords + 1, s.historyWords - index);
            }
            length = std::min(length, end - o);
            words(run, length, 4 * o + lane, 4);
            o += length;
        }
    }
    void words(const uint16_t *p, uint32_t n, uint32_t position, uint32_t step) {
        for (uint32_t done = 0; done < n;) {
            const uint32_t k = std::min(n - done, RecordMoments::Block);
            if (!dsp::accumulateWords(p + done, k, channel_, out_))
                for (uint32_t j = 0; j < k; ++j) code(uint8_t(p[done + j] >> (8 * channel_)), position + (done + j) * step);
            done += k;
        }
    }
    void bytes(const uint8_t *p, uint32_t n, uint32_t position) {
        for (uint32_t done = 0; done < n;) {
            const uint32_t k = std::min(n - done, RecordMoments::Block);
            if (!dsp::accumulateBytes(p + done, k, out_))
                for (uint32_t j = 0; j < k; ++j) code(p[done + j], position + (done + j) * 4);
            done += k;
        }
    }
    // A refused block, code by code; a suspect code takes the frame's correction at its record index.
    void code(uint8_t raw, uint32_t position) {
        int16_t value = int8_t(raw);
        if (AdcBusGlitch::suspect(raw)) {
            const uint32_t index = first_ + (position + m_.ring - start_) % m_.ring;
            frame_.signedCode(Channel(channel_), index, value);
        }
        out_.add(value);
    }
    const FrameView &frame_;
    const FrameMetadata &m_;
    unsigned channel_;
    uint32_t first_, start_;
    dsp::Moments &out_;
};
} // namespace

bool RecordMoments::measure(const FrameView &frame, Channel channel, uint32_t first, uint32_t count, dsp::Moments &out) {
    out = {};
    const auto &m = frame.metadata();
    const unsigned c = unsigned(channel);
    if (!frame.valid() || c > 1 || !m.channels[c].enabled || (m.layout == Layout::InterleavedCh1 && c) ||
        first > m.samples || count > m.samples - first)
        return false;
    if (!count) return true;
    // Past a uint32 wrap of origin + index several indices share a place (as the factory reads them).
    if (m.layout == Layout::InterleavedCh1 || uint64_t(m.origin) + first + count > 0x100000000ULL) {
        auto reader = frame.cursor(channel, first);
        int16_t code;
        for (uint32_t i = 0; i < count && reader.next(code); ++i) out.add(code);
        return out.count == count;
    }
    Walker walker(frame, c, first, out);
    const uint32_t start = uint32_t(m.origin + first) % m.ring, head = std::min(count, m.ring - start);
    walker.positions(start, start + head);
    if (count > head) walker.positions(0, count - head);
    return out.count == count;
}
} // namespace openosc::scope
