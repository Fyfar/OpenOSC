#include "openosc/decode/protocols/i2c.hpp"

namespace openosc::decoding::protocols {
namespace {
// A whole SCL half period under this many points: the unit is FsLow (MinPointsPerBit a clock period).
constexpr uint32_t MinHalfPeriod = (MinPointsPerBit + 1) / 2;
constexpr uint8_t RiseInUnit = 1u << 0, FallInUnit = 1u << 1, FragmentLow = 1u << 2;
constexpr uint32_t Read = 1u << 0, TenBit = 1u << 1, AddressNack = 1u << 2, BeganSr = 1u << 3, EndedSr = 1u << 4,
                   EndedStop = 1u << 5, SummaryMask = 0x00FFFF00u;
}
bool I2cDecoder::configure(const I2cOptions &options) {
    options_ = options;
    configured_ = options.valid();
    return configured_;
}
void I2cDecoder::start(ResultSet &result, int level) {
    writer_.reset();
    queue_.reset(options_.windowPoints, options_.sclChannel);
    sclRise_ = sclFall_ = minHigh_ = byteFirst_ = fragmentFirst_ = fragmentLast_ = 0;
    lastTenBit_ = 0xFFFF; shift_ = 0;
    bits_ = nacked_ = dataBytes_ = 0; firstNack_ = 0xFF;
    sda_ = scl_ = -1;
    phase_ = Phase::Idle;
    fragment_ = fragmentAmbiguous_ = readNack_ = seenFall_ = seenCondition_ = false;
    unit_ = 0;
    earlyCount_ = 0;
    initialLevel(0, level);
    result.initialLevel = int8_t(level);
}
void I2cDecoder::initialLevel(uint8_t channel, int level) {
    const int8_t value = int8_t(level < 0 ? -1 : level ? 1 : 0);
    if (channel == options_.sdaChannel) sda_ = value;
    if (channel == options_.sclChannel) scl_ = value;
}
void I2cDecoder::feed(const Transition *transitions, unsigned count, ResultSet &result) {
    if (!configured_) return;
    CoincidenceQueue::Item item;
    for (unsigned i = 0; i < count; ++i) {
        if (queue_.full() && queue_.pop(item, CoincidenceQueue::Pop::Full)) process(item, result); // A burst: its window is cut.
        queue_.push(transitions[i]);
        while (queue_.pop(item, CoincidenceQueue::Pop::Ready)) process(item, result);
    }
}
void I2cDecoder::process(const CoincidenceQueue::Item &item, ResultSet &result) {
    if (item.t.channel == options_.sclChannel) clock(item.t, item.ambiguous, result);
    else if (item.t.channel == options_.sdaChannel) data(item.t, item.ambiguous, result);
}
void I2cDecoder::mark(uint16_t flags, ResultSet &result) {
    if (phase_ != Phase::Idle) writer_.edit(result).flags |= flags;
}
void I2cDecoder::clock(const Transition &t, bool ambiguous, ResultSet &result) {
    const int8_t level = int8_t(t.level ? 1 : 0);
    if (scl_ < 0) { scl_ = int8_t(level ^ 1); replayEarly(result); }
    if (level == scl_) return;
    scl_ = level;
    if (!level) { // Falling: the high time before it (the shortest one of a transaction is its clock).
        const uint32_t high = t.index - sclRise_;
        if (phase_ != Phase::Idle && (!minHigh_ || high < minHigh_)) minHigh_ = high;
        if ((unit_ & RiseInUnit) && high < MinHalfPeriod) lowRate(result);
        sclFall_ = t.index; seenFall_ = true;
        unit_ = uint8_t(phase_ != Phase::Idle || fragment_ ? unit_ | FallInUnit : unit_ & ~FallInUnit);
        return;
    }
    if ((unit_ & FallInUnit) && t.index - sclFall_ < MinHalfPeriod) lowRate(result);
    if (phase_ != Phase::Idle && seenFall_ && minHigh_ && t.index - sclFall_ > uint64_t(minHigh_) * options_.stretchRatio)
        mark(Note, result); // SCL held low (clock stretching).
    sclRise_ = t.index;
    const int bit = sda_ > 0 ? 1 : 0;
    if (phase_ == Phase::Idle) { // Bits without a START: a fragment.
        if (!fragment_) { fragment_ = true; fragmentFirst_ = t.index; fragmentAmbiguous_ = false; unit_ = 0; }
        unit_ |= RiseInUnit;
        fragmentLast_ = t.index;
        fragmentAmbiguous_ |= ambiguous || sda_ < 0;
        return;
    }
    unit_ |= RiseInUnit;
    if (ambiguous || sda_ < 0) mark(Ambiguous, result);
    // An SDA edge in this edge's window is taken as a data change during SCL low (the order is not
    // known; a condition is never made of it): the bit is the level after it.
    const Transition *next = ambiguous ? queue_.pending(options_.sdaChannel, t.index) : nullptr;
    const int sampled = next ? (next->level ? 1 : 0) : bit;
    if (!bits_) byteFirst_ = t.index;
    shift_ = uint16_t((shift_ << 1) | sampled);
    if (++bits_ == 9) { byte(t.index, result); bits_ = 0; shift_ = 0; }
}
// Both edges of a whole SCL half period lie in the unit (transaction or fragment): the record edge's first
// partial half period and the high around a START / Sr are never measured.
void I2cDecoder::lowRate(ResultSet &result) {
    if (phase_ != Phase::Idle) mark(FsLow, result);
    else if (fragment_) unit_ |= FragmentLow;
}
void I2cDecoder::replayEarly(ResultSet &result) {
    for (unsigned i = 0; i < earlyCount_; ++i) data(early_[i].t, early_[i].ambiguous, result, false);
    earlyCount_ = 0;
}
void I2cDecoder::data(const Transition &t, bool ambiguous, ResultSet &result, bool defer) {
    if (scl_ < 0 && defer) {
        if (earlyCount_ == 4) { // SCL still never moved: the oldest is decided without it.
            const auto oldest = early_[0];
            for (unsigned i = 1; i < 4; ++i) early_[i - 1] = early_[i];
            earlyCount_ = 3;
            data(oldest.t, oldest.ambiguous, result, false);
        }
        early_[earlyCount_++] = {t, ambiguous};
        return;
    }
    const int8_t level = int8_t(t.level ? 1 : 0);
    if (sda_ < 0) sda_ = int8_t(level ^ 1);
    if (level == sda_) return;
    sda_ = level;
    if (scl_ < 0) { // SCL never moved and its level is unknown: no condition can be told.
        if (phase_ != Phase::Idle) mark(Ambiguous, result);
        else if (fragment_) fragmentAmbiguous_ = true;
        return;
    }
    if (scl_ == 0) return; // A data change while SCL is low.
    if (ambiguous) { // Next to an SCL edge: a data change of unknown order, never a condition.
        if (phase_ != Phase::Idle) mark(Ambiguous, result);
        else if (fragment_) fragmentAmbiguous_ = true;
        return;
    }
    condition(t.index, level == 0, result);
}
void I2cDecoder::flushFragment(uint32_t index, ResultSet &result) {
    if (!fragment_) return;
    fragment_ = false;
    DecodeEvent h;
    h.kind = I2cFragment; h.first = fragmentFirst_;
    // Before the record's first condition the START is outside the record; later, clocks without a START.
    h.flags = uint16_t((seenCondition_ ? uint16_t(Framing) : uint16_t(DecodeEvent::Cut)) | (fragmentAmbiguous_ ? Ambiguous : 0) |
                       ((unit_ & FragmentLow) ? FsLow : 0));
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    writer_.begin(result, h);
    writer_.end(result, index > fragmentLast_ ? index : fragmentLast_);
}
void I2cDecoder::condition(uint32_t index, bool isStart, ResultSet &result) {
    if (isStart) {
        const bool repeated = phase_ != Phase::Idle;
        if (repeated) close(index, EndedSr, result);
        else flushFragment(index, result);
        begin(index, repeated, result);
    } else if (phase_ != Phase::Idle) {
        close(index, EndedStop, result);
    } else {
        flushFragment(index, result);
    }
    seenCondition_ = true;
}
void I2cDecoder::begin(uint32_t index, bool repeated, ResultSet &result) {
    DecodeEvent h;
    h.kind = I2cTransaction; h.first = h.last = index;
    h.format = makeFormat(DecodeEvent::Bytes, 8);
    h.aux = (uint32_t(writer_.nextGroup()) << GroupShift) | (repeated ? BeganSr : 0) | (0xFFu << 8);
    writer_.begin(result, h);
    phase_ = Phase::Address;
    bits_ = 0; shift_ = 0; nacked_ = 0; firstNack_ = 0xFF; dataBytes_ = 0;
    minHigh_ = 0; readNack_ = false;
    unit_ = 0; // The SCL edges before the START / Sr are not this unit's.
}
void I2cDecoder::close(uint32_t index, uint32_t endBits, ResultSet &result) {
    auto &e = writer_.edit(result);
    uint16_t flags = 0;
    // Every Sr / STOP follows one SCL rise of its own (bits_ 1). More bits: a condition inside a byte;
    // the address phase not finished: no addressed transfer at all.
    if (bits_ > 1 || phase_ == Phase::Address || phase_ == Phase::AddressLow) flags |= Framing;
    e.aux |= endBits;
    const bool tenBitWrite = (e.aux & TenBit) && !(e.aux & Read) && phase_ == Phase::Data;
    lastTenBit_ = endBits == EndedSr && tenBitWrite ? uint16_t(e.field) : 0xFFFF;
    writer_.end(result, index, flags);
    phase_ = Phase::Idle;
    bits_ = 0; shift_ = 0;
}
void I2cDecoder::byte(uint32_t index, ResultSet &result) {
    const uint8_t value = uint8_t(shift_ >> 1);
    const bool nack = shift_ & 1;
    if (phase_ == Phase::Address) {
        auto &e = writer_.edit(result);
        if ((value & 0xF8) == 0xF0) { // 11110xx: a 10-bit address, its two high bits here.
            const uint32_t high = (value >> 1) & 3u;
            e.aux |= TenBit | (value & 1u);
            if (!(value & 1)) { e.field = high << 8; phase_ = Phase::AddressLow; }
            else {
                if (lastTenBit_ != 0xFFFF && (lastTenBit_ >> 8) == high) e.field = lastTenBit_;
                else { e.field = high << 8; e.flags |= Ambiguous; } // Which device of the 4 x 256 is unknown.
                phase_ = Phase::Data;
            }
        } else {
            e.field = value >> 1; e.aux |= value & 1u;
            phase_ = Phase::Data;
        }
        if (nack) { e.aux |= AddressNack; e.flags |= NoAck; }
        lastTenBit_ = 0xFFFF;
        return;
    }
    if (phase_ == Phase::AddressLow) {
        auto &e = writer_.edit(result);
        e.field |= value;
        if (nack) { e.aux |= AddressNack; e.flags |= NoAck; }
        phase_ = Phase::Data;
        return;
    }
    writer_.push(result, value, byteFirst_, index);
    auto &e = writer_.edit(result);
    const bool read = e.aux & Read;
    if (readNack_) e.flags |= NoAck; // The master NACKed a read byte and still read on.
    if (nack) {
        if (nacked_ < 255) ++nacked_;
        if (firstNack_ == 0xFF) firstNack_ = uint8_t(dataBytes_ < 0xFE ? dataBytes_ : 0xFE);
        if (read) readNack_ = true; // The last read byte's NACK is the normal end.
        else e.flags |= NoAck;
    }
    if (dataBytes_ < 0xFFFF) ++dataBytes_;
    e.aux = (e.aux & ~SummaryMask) | (uint32_t(firstNack_) << 8) | (uint32_t(nacked_) << 16);
}
void I2cDecoder::finish(uint32_t samples, ResultSet &result) {
    if (!configured_) return;
    CoincidenceQueue::Item item;
    while (queue_.pop(item, CoincidenceQueue::Pop::End)) process(item, result);
    replayEarly(result); // SCL never moved: those SDA edges stay unclassified.
    if (phase_ != Phase::Idle) {
        writer_.end(result, samples, DecodeEvent::Cut); // No STOP in the record.
        phase_ = Phase::Idle;
    }
    flushFragment(samples, result);
    clampToRecord(result, samples);
}
} // namespace openosc::decoding::protocols
