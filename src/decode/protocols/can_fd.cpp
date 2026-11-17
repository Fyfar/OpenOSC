#include "openosc/decode/protocols/can_fd.hpp"

namespace openosc::decoding::protocols {
namespace {
using namespace canfd;
constexpr uint16_t SofBit = 0, IdeBit = 13, Crc15Bits = 15, Unknown = 0xFFFF;
uint32_t crcStep(uint32_t crc, int bit, unsigned bits, uint32_t polynomial) {
    const uint32_t top = 1u << (bits - 1), mask = (top << 1) - 1u;
    const bool feedback = ((crc & top) != 0) ^ (bit & 1);
    crc = (crc << 1) & mask;
    return feedback ? crc ^ polynomial : crc;
}
} // namespace

uint64_t canfd::packTrailer(uint32_t received, uint32_t computed, unsigned sbcReceived, unsigned sbcComputed, bool crc21) {
    return (uint64_t(received) & 0x1FFFFF) | ((uint64_t(computed) & 0x1FFFFF) << 21) | (uint64_t(sbcReceived & 15u) << 42) |
           (uint64_t(sbcComputed & 7u) << 46) | (uint64_t(crc21) << 49) | (uint64_t(TrailerVersion) << 50);
}
uint16_t CanFdDecoder::crc15(uint16_t crc, int bit) { return uint16_t(crcStep(crc, bit, 15, 0x4599)); }
uint32_t CanFdDecoder::crc17(uint32_t crc, int bit) { return crcStep(crc, bit, 17, Crc17Polynomial); }
uint32_t CanFdDecoder::crc21(uint32_t crc, int bit) { return crcStep(crc, bit, 21, Crc21Polynomial); }

// The nominal clock decides whether the decoder runs; the data clock is checked by the first FD frame
// with BRS that needs it (a slow record still decodes Classical and BRS = 0 frames).
bool CanFdDecoder::configure(const CanFdOptions &options, const Rational &samplesPerSecond) {
    options_ = options;
    configured_ = options.valid() && BitClock::of(samplesPerSecond, options.bitrate, nominal_);
    dataValid_ = configured_ && BitClock::of(samplesPerSecond, options.dataBitrate, dataClock_);
    return configured_;
}
void CanFdDecoder::start(ResultSet &result, int initialLevel) {
    writer_.reset();
    origin_ = recessiveSince_ = dominantSince_ = frameFirst_ = fragmentFirst_ = fragmentLast_ = 0;
    k_ = 0; tailBit_ = 0; data_ = false;
    level_ = int8_t(initialLevel < 0 ? -1 : (initialLevel ? 1 : 0) ^ int(options_.dominantHigh));
    state_ = State::Integrate; watch_ = Watch::None; fragment_ = false;
    if (level_ == 0) { fragment_ = true; fragmentFirst_ = 0; } // Dominant at the start: inside something.
    result.initialLevel = int8_t(initialLevel);
}
void CanFdDecoder::feed(const Transition *transitions, unsigned count, ResultSet &result) {
    if (!configured_) return;
    for (unsigned i = 0; i < count; ++i)
        edge(transitions[i].index, (transitions[i].level ? 1 : 0) ^ int(options_.dominantHigh), result);
}
void CanFdDecoder::integrate(Watch watch, uint32_t from) {
    state_ = State::Integrate; watch_ = watch; data_ = false;
    if (level_ == 1 && recessiveSince_ < from) recessiveSince_ = from; // Count the 11 bits from here.
}
void CanFdDecoder::edge(uint32_t index, int bit, ResultSet &result) {
    if (level_ < 0) {
        level_ = int8_t(bit ^ 1);
        if (level_ == 0) { fragment_ = true; fragmentFirst_ = 0; }
    }
    if (bit == level_) return;
    if (state_ == State::Frame) sampleUntil(index, result);
    level_ = int8_t(bit);
    if (bit == 1) { // Dominant to recessive: never a synchronisation edge.
        if (state_ == State::Integrate && watch_ != Watch::None) {
            const uint32_t bits = nominal_.bits(index - dominantSince_);
            if (bits >= 6) flag(index, uint16_t(Note | (bits > 12 ? Framing : 0)), result);
        }
        if (state_ == State::Integrate && fragment_) fragmentLast_ = index;
        recessiveSince_ = index;
        return;
    }
    switch (state_) {
    case State::Frame:
        if (phase_ == Phase::Intermission) {
            if (tailBit_ < 2) { integrate(Watch::Overload, index); dominantSince_ = index; return; }
            startFrame(index); // The third intermission bit dominant: SOF.
            return;
        }
        origin_ = index; k_ = 0; // Resynchronisation: the pending bit starts at this edge, in its phase's timing.
        return;
    case State::Idle:
        startFrame(index);
        return;
    case State::Integrate:
        if (index - recessiveSince_ >= nominal_.points(10500)) { // Bus idle (or error delimiter + intermission).
            flushFragment(recessiveSince_, result);
            startFrame(index);
            return;
        }
        dominantSince_ = index;
        if (watch_ == Watch::None) {
            if (!fragment_) { fragment_ = true; fragmentFirst_ = index; }
            fragmentLast_ = index;
        }
        return;
    }
}
void CanFdDecoder::startFrame(uint32_t index) {
    state_ = State::Frame; phase_ = Phase::Stuffed; watch_ = Watch::None; tail_ = Tail::Delimiter;
    origin_ = frameFirst_ = index; k_ = 0; data_ = false;
    bitNo_ = 0; run_ = 0; last_ = -1;
    crc15_ = 0; crc17_ = Crc17Init; crc21_ = Crc21Init; crcReceived_ = 0; aux_ = 0;
    id_ = 0; dlc_ = 0; bytes_ = 0; tailBit_ = 0; stuffCount_ = 0; crcBit_ = 0; sbc_ = 0;
    dlcStart_ = Unknown; crcStart_ = Unknown;
    ide_ = rtr_ = fd_ = brs_ = crcDone_ = ack_ = noAck_ = fsbNext_ = false;
    stop_ = Stop::Arbitration;
}
void CanFdDecoder::sampleUntil(uint32_t end, ResultSet &result) {
    while (state_ == State::Frame) {
        const uint32_t milli = uint32_t(k_) * 1000u;
        const uint32_t position = origin_ + clock().points(milli + samplePoint());
        if (position >= end) return;
        const uint32_t bitEnd = origin_ + clock().points(milli + 1000u);
        ++k_;
        sample(level_, position, bitEnd, result);
    }
}
// The data phase from this sample point: the next sample one data bit later (origin = the data bit's
// start, `1 - SPd` of a data bit after it). No usable data clock: the frame ends here, honestly.
void CanFdDecoder::toData(uint32_t position) {
    data_ = true;
    aux_ |= DataPhase;
    origin_ = position + dataClock_.points(1000u - options_.dataSamplePointPermille);
    k_ = 0;
}
void CanFdDecoder::toNominal(uint32_t position) {
    data_ = false;
    origin_ = position + nominal_.points(1000u - options_.samplePointPermille);
    k_ = 0;
}
void CanFdDecoder::sample(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result) {
    if (phase_ == Phase::Intermission) {
        if (++tailBit_ == 3) state_ = State::Idle; // Three recessive bits: the bus is idle.
        return;
    }
    if (phase_ == Phase::Stuffed && !fd_ && crcDone_ && run_ != 5) phase_ = Phase::Tail; // Classical: after the CRC's stuff bit.
    if (phase_ == Phase::Tail) { tail(bit, position, bitEnd, result); return; }
    if (phase_ == Phase::CrcField) { crcField(bit, position, result); return; }
    // SOF .. data: dynamic stuffing. The FD CRCs take every bit on the bus, stuff bits included.
    if (run_ == 5) { // After five equal bits the next one is a stuff bit of the other value.
        if (bit == last_) { error(position, StuffError, stop_, result); return; }
        last_ = int8_t(bit); run_ = 1;
        stuffCount_ = uint8_t((stuffCount_ + 1) & 7u);
        crc17_ = crc17(crc17_, bit); crc21_ = crc21(crc21_, bit);
        return;
    }
    if (bit == last_) ++run_;
    else { last_ = int8_t(bit); run_ = 1; }
    crc17_ = crc17(crc17_, bit); crc21_ = crc21(crc21_, bit); // An FD frame leaves this phase after its last data bit.
    field(bit, position, result);
}
void CanFdDecoder::field(int bit, uint32_t position, ResultSet &result) {
    const uint16_t n = bitNo_++;
    if (n == SofBit && bit) { state_ = State::Idle; return; } // A glitch, not a SOF.
    if (!fd_ && (crcStart_ == Unknown || n < crcStart_)) crc15_ = crc15(crc15_, bit);
    if (n >= 1 && n <= 11) { id_ = (id_ << 1) | unsigned(bit); return; }
    if (n == 12) { rtr_ = bit; return; } // RTR, SRR of an extended frame, or RRS of an FD base frame.
    if (n == IdeBit) { ide_ = bit; return; }
    if (ide_ && n >= 14 && n <= 31) { id_ = (id_ << 1) | unsigned(bit); return; }
    if (ide_ && n == 32) { rtr_ = bit; return; } // RTR, or RRS of an FD extended frame.
    const uint16_t fdf = ide_ ? 33 : 14;          // Classical r1 (extended) or r0 (base): FDF since ISO 2015.
    if (n == fdf) {
        stop_ = Stop::Control;
        if (!bit) { dlcStart_ = uint16_t(ide_ ? 35 : 15); return; } // Classical: r0 (extended) then the DLC.
        fd_ = true;
        if (rtr_) aux_ |= RrsRecessive;          // An FD frame has no remote frame: noted, not an error.
        dlcStart_ = uint16_t(fdf + 4u);
        return;
    }
    if (fd_) {
        if (n == fdf + 1u) { // res: dominant; recessive is a form error (or a protocol exception).
            if (bit) { aux_ |= ResRecessive; error(position, uint16_t(Framing | Note), Stop::Control, result); }
            return;
        }
        if (n == fdf + 2u) { // BRS: the data phase from this bit's sample point.
            brs_ = bit;
            if (!bit) return;
            aux_ |= Brs;
            if (!dataValid_) { // Fewer than 2 points a data bit (or past the clock's range): not decoded.
                error(position, uint16_t(FsLow | NotChecked), Stop::DataClock, result);
                return;
            }
            toData(position);
            return;
        }
        if (n == fdf + 3u) { if (bit) aux_ |= Esi; return; }
    }
    if (n < dlcStart_) return; // Classical r0 of an extended frame.
    if (n < dlcStart_ + 4u) {
        dlc_ = uint8_t((dlc_ << 1) | bit);
        if (n == dlcStart_ + 3u) {
            const unsigned bytes = fd_ ? length(dlc_) : (rtr_ ? 0u : (dlc_ > 8 ? 8u : dlc_));
            crcStart_ = uint16_t(dlcStart_ + 4u + bytes * 8u);
            if (fd_) {
                aux_ |= DlcKnown | (dlc_ > 10 ? Crc21 : 0u);
                stop_ = Stop::Data;
                if (!bytes) { phase_ = Phase::CrcField; fsbNext_ = true; stop_ = Stop::StuffCount; }
            }
        }
        return;
    }
    if (n < crcStart_) { // Data.
        const unsigned at = n - (dlcStart_ + 4u);
        data_bytes_[at / 8] = uint8_t((data_bytes_[at / 8] << 1) | bit);
        if (at % 8 == 7) bytes_ = uint8_t(at / 8 + 1);
        if (fd_ && n + 1u == crcStart_) { phase_ = Phase::CrcField; fsbNext_ = true; stop_ = Stop::StuffCount; }
        return;
    }
    // Classical CRC (FD frames are in the CRC field phase by now).
    crcReceived_ = (crcReceived_ << 1) | unsigned(bit);
    if (n == crcStart_ + Crc15Bits - 1u) crcDone_ = true;
}
// The FD CRC field: a fixed stuff bit (the inverse of the bit before it) before the stuff count and after
// every fourth bit; the stuff count (G2 G1 G0 P) goes into the CRC, the fixed stuff bits do not.
void CanFdDecoder::crcField(int bit, uint32_t position, ResultSet &result) {
    if (fsbNext_) {
        if (bit == last_) { error(position, Framing, stop_, result); return; }
        last_ = int8_t(bit); fsbNext_ = false;
        return;
    }
    last_ = int8_t(bit);
    const unsigned i = crcBit_++, crcBits = (aux_ & Crc21) ? 21u : 17u, total = 4u + crcBits;
    if (i < 4) {
        sbc_ = uint8_t((sbc_ << 1) | unsigned(bit));
        crc17_ = crc17(crc17_, bit); crc21_ = crc21(crc21_, bit);
        if (i == 3) { aux_ |= SbcReceived; stop_ = Stop::Crc; }
    } else {
        crcReceived_ = (crcReceived_ << 1) | unsigned(bit);
    }
    if (crcBit_ == total) { crcDone_ = true; phase_ = Phase::Tail; tail_ = Tail::Delimiter; stop_ = Stop::CrcDelimiter; return; }
    if (crcBit_ % 4u == 0) fsbNext_ = true;
}
void CanFdDecoder::tail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result) {
    if (fd_) fdTail(bit, position, bitEnd, result);
    else classicTail(bit, position, bitEnd, result);
}
// Classical: one CRC delimiter, one ACK slot, one ACK delimiter, EOF; strict, as CanDecoder.
void CanFdDecoder::classicTail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result) {
    const uint8_t t = tailBit_++;
    if (t == 0 || t == 2) { if (!bit) error(position, Framing, t ? Stop::AckDelimiter : Stop::CrcDelimiter, result); return; }
    if (t == 1) { ack_ = !bit; noAck_ = bit; return; } // ACK slot.
    if (t < 9) { if (!bit) error(position, Framing, Stop::Eof, result); return; } // EOF 1..6.
    emit(bitEnd, 0, result);
    if (bit) { phase_ = Phase::Intermission; tailBit_ = 0; return; } // EOF 7 recessive: complete.
    integrate(Watch::Overload, position); // A dominant last EOF bit does not void the frame: an overload follows.
    dominantSince_ = origin_;
}
// ISO CAN FD: after the CRC delimiter a second recessive delimiter bit, and after the ACK slot a second
// dominant ACK bit, are normal (phase shifts of the switch back). Read after the first delimiter bit
// b1, b2, ...:
//   d ...      ACK at b1 (and b2 if dominant, then b3 the ACK delimiter, else b2 it);
//   r r        b1 a recessive ACK slot (NoAck), b2 the ACK delimiter;
//   r d r      b1 a second delimiter bit, ACK b2, b3 the ACK delimiter;
//   r d d r    b1 a second delimiter bit, ACK b2 b3, b4 the ACK delimiter;
//   r d d d    no tolerated form has three dominant bits there: b1 a recessive ACK slot and an error flag
//              from b2 (NoAck, Framing at the ACK delimiter).
void CanFdDecoder::fdTail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result) {
    switch (tail_) {
    case Tail::Delimiter:
        if (!bit) { error(position, Framing, Stop::CrcDelimiter, result); return; }
        if (data_) toNominal(position);
        tail_ = Tail::AfterDelimiter; stop_ = Stop::AckSlot;
        return;
    case Tail::AfterDelimiter:
        if (!bit) { ack_ = true; tail_ = Tail::AckSecond; stop_ = Stop::AckDelimiter; }
        else tail_ = Tail::Recessive;
        return;
    case Tail::AckSecond:
        if (!bit) { aux_ |= Ack2; tail_ = Tail::AckDelimiter; return; }
        tail_ = Tail::Eof; tailBit_ = 0; stop_ = Stop::Eof; // This bit was the ACK delimiter.
        return;
    case Tail::AckDelimiter:
        if (!bit) { error(position, Framing, Stop::AckDelimiter, result); return; }
        tail_ = Tail::Eof; tailBit_ = 0; stop_ = Stop::Eof;
        return;
    case Tail::Recessive:
        if (bit) { noAck_ = true; tail_ = Tail::Eof; tailBit_ = 0; stop_ = Stop::Eof; return; }
        tail_ = Tail::RecessiveDominant;
        return;
    case Tail::RecessiveDominant:
        if (bit) { aux_ |= CrcDelimiter2; ack_ = true; tail_ = Tail::Eof; tailBit_ = 0; stop_ = Stop::Eof; return; }
        tail_ = Tail::RecessiveDominant2;
        return;
    case Tail::RecessiveDominant2:
        if (bit) { aux_ |= CrcDelimiter2 | Ack2; ack_ = true; tail_ = Tail::Eof; tailBit_ = 0; stop_ = Stop::Eof; return; }
        noAck_ = true;
        error(position, Framing, Stop::AckDelimiter, result);
        return;
    case Tail::Eof: eof(bit, position, bitEnd, result); return;
    }
}
void CanFdDecoder::eof(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result) {
    const uint8_t t = tailBit_++;
    if (t < 6) { if (!bit) error(position, Framing, Stop::Eof, result); return; } // EOF 1..6.
    stop_ = Stop::Complete;
    emit(bitEnd, 0, result);
    if (bit) { phase_ = Phase::Intermission; tailBit_ = 0; return; }
    integrate(Watch::Overload, position); // EOF 7 dominant: an overload follows.
    dominantSince_ = origin_;
}
void CanFdDecoder::emit(uint32_t last, uint16_t flags, ResultSet &result) {
    DecodeEvent h;
    h.kind = CanFrame; h.first = frameFirst_;
    const bool dlcKnown = crcStart_ != Unknown;
    if (nominal_.low()) flags |= FsLow;
    if (!fd_) { // As CanDecoder.
        h.field = id_ | (uint32_t(ide_) << 29) | (uint32_t(rtr_) << 30);
        h.format = makeFormat(bytes_ ? DecodeEvent::Bytes : DecodeEvent::None, dlcKnown ? dlc_ : 0);
        if (crcDone_) {
            h.aux = (crcReceived_ & 0x7FFF) | (uint32_t(crc15_) << 15);
            if ((crcReceived_ & 0x7FFF) != crc15_) flags |= CheckFailed;
            if (noAck_) flags |= NoAck;
        } else {
            flags |= NotChecked;
        }
        if (dlcKnown && dlc_ > 8) flags |= Note;
        h.flags = flags;
        writer_.begin(result, h);
        for (unsigned i = 0; i < bytes_; ++i) writer_.push(result, data_bytes_[i], frameFirst_, last);
        writer_.end(result, last);
        return;
    }
    uint32_t aux = aux_ | (uint32_t(stuffCount_ & 7u) << SbcCalculatedShift) | (uint32_t(stop_) << StopShift);
    if (aux & SbcReceived) aux |= sbc_ & SbcReceivedMask;
    uint8_t trailer[TrailerBytes];
    if (crcDone_) {
        const bool wide = aux & Crc21;
        const uint32_t computed = wide ? crc21_ : crc17_;
        const unsigned g = (sbc_ >> 1) & 7u, parity = sbc_ & 1u;
        if (parity != (((g >> 2) ^ (g >> 1) ^ g) & 1u)) { aux |= SbcParity; flags |= CheckFailed; }
        if (fromGray(g) != stuffCount_) { aux |= SbcMismatch; flags |= CheckFailed; }
        if (crcReceived_ != computed) flags |= CheckFailed;
        if (noAck_) flags |= NoAck;
        aux |= Trailer;
        const uint64_t packed = packTrailer(crcReceived_, computed, sbc_, stuffCount_, wide);
        for (unsigned i = 0; i < TrailerBytes; ++i) trailer[i] = uint8_t(packed >> (8 * i));
    } else {
        flags |= NotChecked;
    }
    if (aux & RrsRecessive) flags |= Note;
    if ((aux & DataPhase || aux & Brs) && dataValid_ && dataClock_.low()) flags |= FsLow;
    h.field = id_ | (uint32_t(ide_) << 29) | Fdf;
    h.aux = aux;
    h.format = makeFormat(bytes_ || crcDone_ ? DecodeEvent::Bytes : DecodeEvent::None, dlcKnown ? dlc_ : 0);
    h.flags = flags; h.last = last;
    writer_.block(result, h, data_bytes_, bytes_, crcDone_ ? trailer : nullptr, crcDone_ ? TrailerBytes : 0);
}
// An error: the frame as received so far, then the bus watched for its error flag at the nominal rate
// (a receiver switches back at the sample point where it detects the error, Bosch iCC 2013).
void CanFdDecoder::error(uint32_t position, uint16_t flags, Stop stop, ResultSet &result) {
    stop_ = stop;
    emit(position, flags, result);
    integrate(Watch::ErrorFlag, position);
    if (level_ == 0) dominantSince_ = origin_; // The flag may already be on the bus.
}
void CanFdDecoder::flag(uint32_t last, uint16_t flags, ResultSet &result) {
    DecodeEvent h;
    h.kind = watch_ == Watch::Overload ? CanOverload : CanErrorFlag;
    h.first = dominantSince_; h.aux = nominal_.bits(last - dominantSince_);
    h.flags = uint16_t(flags | (nominal_.low() ? FsLow : 0));
    writer_.begin(result, h);
    writer_.end(result, last);
}
void CanFdDecoder::flushFragment(uint32_t last, ResultSet &result) {
    if (!fragment_) return;
    fragment_ = false;
    if (watch_ != Watch::None) return;
    DecodeEvent h;
    h.kind = CanFragment; h.first = fragmentFirst_; h.flags = DecodeEvent::Cut;
    writer_.begin(result, h);
    writer_.end(result, last > fragmentLast_ ? last : fragmentLast_);
}
void CanFdDecoder::finish(uint32_t samples, ResultSet &result) {
    if (!configured_) return;
    if (state_ == State::Frame) {
        sampleUntil(samples, result);
        if (state_ == State::Frame && phase_ != Phase::Intermission) emit(samples, DecodeEvent::Cut, result);
        if (state_ == State::Frame) state_ = State::Idle;
    }
    if (state_ == State::Integrate && watch_ != Watch::None && level_ == 0 && nominal_.bits(samples - dominantSince_) >= 6)
        flag(samples, uint16_t(Note | DecodeEvent::Open), result);
    flushFragment(samples, result);
    clampToRecord(result, samples);
}
} // namespace openosc::decoding::protocols
