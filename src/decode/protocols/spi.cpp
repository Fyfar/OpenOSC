#include "openosc/decode/protocols/spi.hpp"

namespace openosc::decoding::protocols {
namespace {
// A whole SCLK half period under this many points: the frame's bits are FsLow (MinPointsPerBit a period).
constexpr uint32_t MinHalfPeriod = (MinPointsPerBit + 1) / 2;
}
bool SpiDecoder::configure(const SpiOptions &options, const Rational &samplesPerSecond) {
    options_ = options;
    configured_ = options.valid() && (options.align == SpiOptions::Align::Manual || samplesPerSecond.valid());
    gap_ = options.gapNs ? pointsOfNs(samplesPerSecond, options.gapNs) : 0;
    if (options.gapNs && !gap_) configured_ = false;
    return configured_;
}
void SpiDecoder::start(ResultSet &result, int level) {
    writer_.reset();
    queue_.reset(options_.windowPoints, options_.clockChannel);
    lastEdge_ = wordFirst_ = 0; word_ = 0; bits_ = 0;
    clock_ = data_ = -1;
    seenEdge_ = frame_ = manualDone_ = false;
    initialLevel(0, level);
    result.initialLevel = int8_t(level);
}
void SpiDecoder::initialLevel(uint8_t channel, int level) {
    const int8_t value = int8_t(level < 0 ? -1 : level ? 1 : 0);
    if (channel == options_.clockChannel) clock_ = value;
    if (channel == options_.dataChannel) data_ = value;
}
void SpiDecoder::feed(const Transition *transitions, unsigned count, ResultSet &result) {
    if (!configured_) return;
    CoincidenceQueue::Item item;
    for (unsigned i = 0; i < count; ++i) {
        if (queue_.full() && queue_.pop(item, CoincidenceQueue::Pop::Full)) process(item, result);
        queue_.push(transitions[i]);
        while (queue_.pop(item, CoincidenceQueue::Pop::Ready)) process(item, result);
    }
}
void SpiDecoder::process(const CoincidenceQueue::Item &item, ResultSet &result) {
    const int8_t level = int8_t(item.t.level ? 1 : 0);
    if (item.t.channel == options_.clockChannel) { clock(item.t, item.ambiguous, result); return; }
    if (item.t.channel != options_.dataChannel) return;
    data_ = level; // Sampled at the next sampling edge; closeness is the queue's.
}
void SpiDecoder::open(uint32_t index, bool aligned, bool idleOk, ResultSet &result) {
    DecodeEvent h;
    h.kind = SpiFrame; h.first = h.last = index;
    h.format = makeFormat(options_.bits > 8 ? DecodeEvent::Words16 : DecodeEvent::Bytes, options_.bits);
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    h.flags = uint16_t((aligned ? 0 : Ambiguous) | (idleOk ? 0 : Framing));
    writer_.begin(result, h);
    frame_ = true; word_ = 0; bits_ = 0;
}
void SpiDecoder::close(uint32_t last, uint16_t flags, ResultSet &result) {
    if (!frame_) return;
    frame_ = false;
    if (bits_) { // A partial word: not stored, its bits kept in aux.
        auto &e = writer_.edit(result);
        e.aux = (e.aux & ~31u) | bits_;
        flags |= DecodeEvent::Cut;
    }
    writer_.end(result, last, flags);
    bits_ = 0; word_ = 0;
}
void SpiDecoder::clock(const Transition &t, bool ambiguous, ResultSet &result) {
    const int8_t level = int8_t(t.level ? 1 : 0);
    if (clock_ < 0) clock_ = int8_t(level ^ 1);
    if (level == clock_) return;
    const int8_t prior = clock_;
    clock_ = level;
    const bool manual = options_.align == SpiOptions::Align::Manual;
    if (manual && t.index < options_.manualStart) { lastEdge_ = t.index; seenEdge_ = true; return; }
    // The idle before this edge: since the previous edge, or since the record began.
    const uint32_t idle = seenEdge_ ? t.index - lastEdge_ : t.index;
    const bool pause = gap_ && idle > gap_;
    if (frame_ && pause) close(lastEdge_, 0, result);
    if (!frame_) {
        const bool aligned = manual ? (!manualDone_ || pause) : pause;
        open(t.index, aligned, prior == int8_t(options_.cpol), result);
        manualDone_ = true;
    } else if (idle < MinHalfPeriod) { // A whole half period of this frame (its first edge has none before it).
        writer_.edit(result).flags |= FsLow;
    }
    lastEdge_ = t.index; seenEdge_ = true;
    const bool sampling = (level == 1) == (options_.cpol == options_.cpha);
    if (!sampling) return;
    if (ambiguous || data_ < 0) writer_.edit(result).flags |= Ambiguous;
    const unsigned bit = data_ > 0 ? 1u : 0u;
    if (!bits_) wordFirst_ = t.index;
    word_ = options_.msbFirst ? uint16_t((word_ << 1) | bit) : uint16_t(word_ | (bit << bits_));
    if (++bits_ == options_.bits) {
        writer_.push(result, word_, wordFirst_, t.index);
        bits_ = 0; word_ = 0;
    }
}
void SpiDecoder::finish(uint32_t samples, ResultSet &result) {
    if (!configured_) return;
    CoincidenceQueue::Item item;
    while (queue_.pop(item, CoincidenceQueue::Pop::End)) process(item, result);
    // Whole words with too short a quiet tail may go on past the record.
    const bool open = frame_ && !bits_ && (!gap_ || samples - lastEdge_ <= gap_);
    close(lastEdge_, open ? DecodeEvent::Open : 0, result);
    clampToRecord(result, samples);
}
} // namespace openosc::decoding::protocols
