#include "openosc/decode/protocols/lin.hpp"

namespace openosc::decoding::protocols {
namespace {
constexpr uint32_t Enhanced = 1u << 16, LengthKnown = 1u << 17;
uint32_t distance(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }
}
bool LinOptions::valid() const {
    if (!baud || breakBits < 10 || breakBits > 40 || syncTolerancePermille > 500) return false;
    for (const uint8_t length : lengths) if (length > 8) return false;
    return true;
}
uint8_t LinDecoder::protectedId(uint8_t id) {
    id &= 0x3F;
    const unsigned b = id;
    const unsigned p0 = (b ^ (b >> 1) ^ (b >> 2) ^ (b >> 4)) & 1u;
    const unsigned p1 = ~((b >> 1) ^ (b >> 3) ^ (b >> 4) ^ (b >> 5)) & 1u;
    return uint8_t(id | (p0 << 6) | (p1 << 7));
}
uint8_t LinDecoder::checksum(const uint8_t *data, unsigned count, uint8_t seed) {
    unsigned sum = seed;
    for (unsigned i = 0; i < count; ++i) { sum += data[i]; if (sum > 255) sum -= 255; }
    return uint8_t(~sum);
}
bool LinDecoder::configure(const LinOptions &options, const Rational &samplesPerSecond) {
    options_ = options;
    configured_ = options.valid() && BitClock::of(samplesPerSecond, options.baud, nominal_);
    breakPoints_ = configured_ ? nominal_.points(options.breakBits * 1000u) : 0;
    format_ = CharacterFormat{};
    return configured_;
}
void LinDecoder::start(ResultSet &result, int initialLevel) {
    writer_.reset(); rx_.cancel();
    clock_ = nominal_;
    spaceSince_ = breakEnd_ = lastEnd_ = fragmentFirst_ = 0;
    sum_ = 0; syncCount_ = pid_ = length_ = data_ = 0;
    level_ = int8_t(initialLevel < 0 ? -1 : initialLevel ? 1 : 0);
    state_ = State::Hunt;
    frame_ = pidSeen_ = enhanced_ = pendingSpace_ = fragment_ = syncChecked_ = false;
    result.initialLevel = int8_t(initialLevel);
}
void LinDecoder::feed(const Transition *transitions, unsigned count, ResultSet &result) {
    if (!configured_) return;
    for (unsigned i = 0; i < count; ++i) edge(transitions[i].index, transitions[i].level ? 1 : 0, result);
}
void LinDecoder::flushFragment(uint32_t end, ResultSet &result, bool cut) {
    if (!fragment_) return;
    fragment_ = false;
    if (fragmentFirst_ >= end) return; // Only the break's own edge.
    DecodeEvent h;
    h.kind = LinFragment; h.first = fragmentFirst_;
    // Before the first break, or still at space past the record's end (a break or a byte not seen whole):
    // Cut; between frames and ended within the record: Framing.
    h.flags = uint16_t(state_ == State::Hunt || cut ? uint16_t(DecodeEvent::Cut) : uint16_t(Framing));
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    writer_.begin(result, h);
    writer_.end(result, end);
}
void LinDecoder::closeFrame(uint32_t last, uint16_t flags, ResultSet &result) {
    if (!frame_) return;
    frame_ = false;
    writer_.end(result, last, flags);
}
void LinDecoder::edge(uint32_t index, int level, ResultSet &result) {
    if (level_ < 0) level_ = int8_t(level ^ 1);
    if (level == level_) return;
    if (rx_.active() && rx_.advance(index, level_)) character(result);
    if ((state_ == State::Hunt || state_ == State::Between) && !fragment_) { fragment_ = true; fragmentFirst_ = index; }
    if (level == 0) {
        spaceSince_ = index;
        level_ = 0;
        if (state_ == State::Delimiter) {
            const uint16_t flags = index - breakEnd_ < nominal_.points(500) ? Framing : 0; // Delimiter under half a bit.
            if (flags) writer_.edit(result).flags |= flags;
            state_ = State::Sync; syncCount_ = 0;
        }
        if (state_ == State::Sync) { sync(index, result); return; }
        if (state_ == State::Bytes && !rx_.active() && !pendingSpace_) {
            if (!syncChecked_) { // SYNC's stop bit: nothing may fall before 9.5 bits after its START.
                syncChecked_ = true;
                if (index < syncEdges_[0] + clock_.points(9500)) writer_.edit(result).flags |= SyncError;
            }
            rx_.begin(format_, clock_, index);
        }
        return;
    }
    level_ = 1;
    if (index - spaceSince_ >= breakPoints_) { onBreak(spaceSince_, index, result); return; }
    if (pendingSpace_) { // Space through the stop bit but no break: a 0x00 with a framing error.
        pendingSpace_ = false;
        store(0, true, rx_.start(), rx_.end(), result);
    }
    if (state_ == State::Sync) sync(index, result);
}
void LinDecoder::onBreak(uint32_t first, uint32_t end, ResultSet &result) {
    rx_.cancel(); pendingSpace_ = false;
    if (frame_) {
        uint16_t flags = 0;
        if (state_ != State::Bytes || !pidSeen_) flags = Framing | NotChecked; // The header never completed.
        else if (length_ && !data_) flags = NoAck | NotChecked;                // No response.
        else if (length_) flags = Framing | NotChecked;                        // A short response.
        closeFrame(lastEnd_ > first ? first : lastEnd_ ? lastEnd_ : first, flags, result);
    }
    flushFragment(first, result);
    DecodeEvent h;
    h.kind = LinFrame; h.first = h.last = first;
    h.format = makeFormat(DecodeEvent::Bytes, 8);
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    h.flags = uint16_t(nominal_.low() ? FsLow : 0);
    writer_.begin(result, h);
    frame_ = true; pidSeen_ = false; syncChecked_ = false;
    clock_ = nominal_; breakEnd_ = lastEnd_ = end;
    sum_ = 0; length_ = data_ = 0; syncCount_ = 0;
    state_ = State::Delimiter;
}
void LinDecoder::sync(uint32_t index, ResultSet &result) {
    syncEdges_[syncCount_++] = index;
    if (syncCount_ < 10) return;
    // 0x55 with START and stop: an edge at every bit boundary 0..9 from the START's falling edge.
    const uint32_t start = syncEdges_[0], span = syncEdges_[8] - start;
    const BitClock measured = BitClock::measured(span, 8);
    bool ok = measured.pointsQ16 * 1000u >= nominal_.pointsQ16 * (1000u - options_.syncTolerancePermille) &&
              measured.pointsQ16 * 1000u <= nominal_.pointsQ16 * (1000u + options_.syncTolerancePermille);
    for (unsigned k = 1; ok && k < 10; ++k)
        ok = distance(syncEdges_[k], start + measured.points(k * 1000u)) <= measured.points(250);
    lastEnd_ = start + measured.points(10000);
    if (!ok) { // No bit time to trust: the frame ends here, its checksum never compared.
        closeFrame(syncEdges_[9], uint16_t(SyncError | NotChecked), result);
        state_ = State::Between;
        return;
    }
    clock_ = measured;
    if (clock_.low()) writer_.edit(result).flags |= FsLow;
    state_ = State::Bytes;
}
void LinDecoder::character(ResultSet &result) {
    if (rx_.falseStart()) return;
    if (rx_.allSpace() && !rx_.stopOk()) { pendingSpace_ = true; return; } // A break, or 0x00 with FE.
    store(uint8_t(rx_.value()), !rx_.stopOk(), rx_.start(), rx_.end(), result);
}
void LinDecoder::store(uint8_t value, bool framing, uint32_t first, uint32_t last, ResultSet &result) {
    if (state_ != State::Bytes || !frame_) return;
    lastEnd_ = last;
    if (framing) writer_.edit(result).flags |= Framing;
    if (!pidSeen_) {
        pidSeen_ = true; pid_ = value;
        auto &e = writer_.edit(result);
        e.field = value;
        const uint8_t id = value & 0x3F;
        const bool parityOk = protectedId(id) == value;
        if (!parityOk) e.flags |= ParityError;
        length_ = parityOk ? options_.lengths[id] : 0;
        enhanced_ = options_.checksum == LinOptions::Checksum::Enhanced && id != 0x3C && id != 0x3D;
        sum_ = enhanced_ ? value : 0;
        if (enhanced_) e.aux |= Enhanced;
        if (length_) e.aux |= LengthKnown;
        else e.flags |= LengthUnknown | NotChecked;
        return;
    }
    if (!length_ || data_ < length_) { // A data byte (or a raw one of unknown length).
        writer_.push(result, value, first, last);
        if (length_) { ++data_; sum_ = uint16_t(sum_ + value); if (sum_ > 255) sum_ = uint16_t(sum_ - 255); }
        else if (data_ < 255) ++data_;
        return;
    }
    const uint8_t computed = uint8_t(~sum_);
    auto &e = writer_.edit(result);
    e.aux |= uint32_t(value) | (uint32_t(computed) << 8);
    closeFrame(last, value == computed ? 0 : CheckFailed, result);
    state_ = State::Between;
}
void LinDecoder::finish(uint32_t samples, ResultSet &result) {
    if (!configured_) return;
    if (rx_.active() && rx_.advance(samples, level_)) character(result);
    if (level_ == 0 && samples - spaceSince_ >= breakPoints_) { // A break still at the record's end.
        onBreak(spaceSince_, samples, result);
        closeFrame(samples, uint16_t(DecodeEvent::Cut | DecodeEvent::Open | NotChecked), result);
        clampToRecord(result, samples);
        return;
    }
    if (pendingSpace_) { pendingSpace_ = false; store(0, true, rx_.start(), rx_.end(), result); }
    if (frame_) {
        if (rx_.active()) closeFrame(samples, uint16_t(DecodeEvent::Cut | NotChecked), result); // A byte past the end.
        else if (state_ == State::Bytes && pidSeen_ && !length_) closeFrame(lastEnd_, DecodeEvent::Open, result);
        else closeFrame(samples, uint16_t(DecodeEvent::Cut | NotChecked), result);
    }
    rx_.cancel();
    flushFragment(samples, result, level_ == 0);
    clampToRecord(result, samples);
}
} // namespace openosc::decoding::protocols
