#include "openosc/decode/protocols/uart.hpp"

namespace openosc::decoding::protocols {
void CharacterReceiver::begin(const CharacterFormat &format, const BitClock &clock, uint32_t start) {
    format_ = format; clock_ = clock; start_ = start;
    value_ = 0; step_ = 0; ones_ = 0;
    stopStep_ = uint8_t(1 + format.dataBits + (format.parity != Parity::None));
    // The configured stop is checked whole: 1.5 and 2 stop bits are sampled a second time.
    steps_ = uint8_t(stopStep_ + 1 + (format.stopHalfBits > 2 ? 1 : 0));
    cut_ = 0;
    active_ = true; falseStart_ = cutShort_ = incomplete_ = false; parityOk_ = stopOk_ = allSpace_ = true;
}
uint32_t CharacterReceiver::position(unsigned step) const {
    if (step <= stopStep_) return start_ + clock_.points(step * 1000u + 500u);
    // The second stop sample: the centre of the half bit (1.5) or of the second bit (2).
    return start_ + clock_.points((stopStep_ + 1u) * 1000u + (format_.stopHalfBits == 3 ? 250u : 500u));
}
void CharacterReceiver::cutStop(uint32_t at) {
    cutFirstStopOk_ = stopOk_; // Only the first stop bit was sampled yet.
    stopOk_ = false; cutShort_ = true; cut_ = at; active_ = false;
}
void CharacterReceiver::sample(unsigned step, int bit) {
    if (bit) allSpace_ = false;
    if (step == 0) { // START must still be at space at its centre.
        if (bit) { falseStart_ = true; active_ = false; }
        return;
    }
    const unsigned data = format_.dataBits, parityStep = data + 1u;
    if (step <= data) {
        if (format_.msbFirst) value_ = uint16_t((value_ << 1) | bit);
        else value_ = uint16_t(value_ | (bit << (step - 1)));
        ones_ = uint8_t(ones_ + bit);
        return;
    }
    if (format_.parity != Parity::None && step == parityStep) {
        switch (format_.parity) {
        case Parity::Even: parityOk_ = ((ones_ + bit) & 1) == 0; break;
        case Parity::Odd: parityOk_ = ((ones_ + bit) & 1) == 1; break;
        case Parity::Mark: parityOk_ = bit == 1; break;
        default: parityOk_ = bit == 0; break;
        }
        return;
    }
    if (!bit) stopOk_ = false;
}
bool CharacterReceiver::advance(uint32_t end, int level) {
    while (active_ && position(step_) < end) {
        sample(step_, level > 0 ? 1 : 0);
        if (++step_ == steps_) active_ = false;
        if (!active_) return true;
    }
    return false;
}

bool CharacterReceiver::inLaterStop() const { return active_ && step_ > stopStep_; }
bool UartDecoder::configure(const UartOptions &options, const Rational &samplesPerSecond) {
    options_ = options;
    configured_ = options.valid() && BitClock::of(samplesPerSecond, options.baud, clock_);
    if (!configured_) return false;
    frame_ = clock_.points(options.format.milliBits());
    groupGap_ = clock_.points(uint32_t(options.groupIdleBits > 4000 ? 4000 : options.groupIdleBits) * 1000u);
    // Half a bit <= the uncertainty, in Q16 points: pointsQ16 / 2 <= uncertainty << 16.
    timingUnsure_ = options.timingUncertaintyPoints && clock_.pointsQ16 <= (uint64_t(options.timingUncertaintyPoints) << 17);
    return true;
}
void UartDecoder::start(ResultSet &result, int initialLevel) {
    writer_.reset(); rx_.cancel();
    idleSince_ = spaceSince_ = groupEnd_ = breakStart_ = falseStarts_ = 0;
    synced_ = unsure_ = inBreak_ = spaceAtStart_ = group_ = false; groupFlags_ = 0;
    level_ = initialLevel < 0 ? -1 : (initialLevel ? 1 : 0) ^ int(options_.inverted);
    spaceAtStart_ = level_ == 0;
    result.initialLevel = int8_t(initialLevel);
}
void UartDecoder::feed(const Transition *transitions, unsigned count, ResultSet &result) {
    if (!configured_) return;
    for (unsigned i = 0; i < count; ++i)
        edge(transitions[i].index, (transitions[i].level ? 1 : 0) ^ int(options_.inverted), result);
}
void UartDecoder::edge(uint32_t index, int level, ResultSet &result) {
    if (level_ < 0) { // The first edge tells the level before it, held since the record began.
        level_ = level ^ 1;
        spaceAtStart_ = level_ == 0;
    }
    if (level == level_) return;
    if (rx_.active() && rx_.advance(index, level_)) character(result);
    else if (level == 0 && rx_.inLaterStop()) { rx_.cutStop(index); character(result); } // START too early.
    if (level == 0) {
        spaceSince_ = index;
        if (!rx_.active() && !inBreak_ && !spaceAtStart_) {
            unsure_ = !synced_ && index - idleSince_ < frame_;
            holdTo(index, result);
            rx_.begin(options_.format, clock_, index);
        }
    } else {
        if (inBreak_) { inBreak_ = false; synced_ = true; single(result, UartBreak, breakStart_, index, 0); }
        else if (spaceAtStart_) single(result, index >= frame_ ? UartBreak : UartFragment, 0, index, DecodeEvent::Cut);
        spaceAtStart_ = false;
        idleSince_ = index;
    }
    level_ = level;
}
void UartDecoder::holdTo(uint32_t start, ResultSet &result) {
    // A START before the previous character's configured end (after a framing error) ends it there:
    // no event reaches past the next one's first point.
    if (group_) {
        if (groupEnd_ > start) { groupEnd_ = start; writer_.edit(result).last = start; }
        return;
    }
    auto *e = result.lastEvent();
    if (e && e->kind == UartData && e->last > start && !writer_.exhausted()) e->last = start;
}
void UartDecoder::headerAt(ResultSet &result, uint32_t first, uint16_t flags) {
    DecodeEvent h;
    h.kind = UartData; h.first = h.last = first;
    h.format = makeFormat(options_.format.dataBits > 8 ? DecodeEvent::Words16 : DecodeEvent::Bytes, options_.format.dataBits);
    h.flags = uint16_t(flags | (clock_.low() ? FsLow : 0));
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    writer_.begin(result, h);
}
void UartDecoder::single(ResultSet &result, uint8_t kind, uint32_t first, uint32_t last, uint16_t flags) {
    closeGroup(result);
    DecodeEvent h;
    h.kind = kind; h.first = first;
    h.flags = uint16_t(flags | (clock_.low() ? FsLow : 0));
    h.aux = uint32_t(writer_.nextGroup()) << GroupShift;
    writer_.begin(result, h);
    writer_.end(result, last);
}
void UartDecoder::closeGroup(ResultSet &result) {
    if (!group_) return;
    group_ = false;
    writer_.end(result, groupEnd_);
}
void UartDecoder::character(ResultSet &result) {
    if (rx_.falseStart()) { ++falseStarts_; return; } // A space shorter than half a bit: no START.
    if (rx_.allSpace() && !rx_.stopOk()) { // Space through the stop bits: a break until the line idles.
        closeGroup(result);
        inBreak_ = true; breakStart_ = rx_.start();
        return;
    }
    uint16_t flags = rx_.incomplete() ? DecodeEvent::Cut : 0;
    if (!rx_.parityOk()) flags |= ParityError;
    if (!rx_.stopOk()) flags |= Framing;
    if (unsure_ || timingUnsure_) flags |= Ambiguous; // The phase (unsure_) and the edge timing apart.
    // A known phase stays known through a right stop, or a START that cut a stop whose first bit was at
    // mark (that START is the edge that ends it); a stop at space loses it. An unsure START never makes
    // it known: only a whole frame of idle or a complete break does.
    synced_ = !unsure_ && (rx_.stopOk() || (rx_.stopCut() && rx_.firstStopOk()));
    const uint32_t first = rx_.start(), last = rx_.end();
    if ((flags & ~uint16_t(Ambiguous)) || !groupGap_) {
        closeGroup(result);
        headerAt(result, first, flags);
        writer_.push(result, rx_.value(), first, last);
        writer_.end(result, last);
        return;
    }
    if (!group_ || flags != groupFlags_ || first - groupEnd_ > groupGap_ || writer_.elements() >= 255) {
        closeGroup(result);
        headerAt(result, first, flags);
        group_ = true; groupFlags_ = flags;
    }
    writer_.push(result, rx_.value(), first, last);
    groupEnd_ = last;
}
void UartDecoder::finish(uint32_t samples, ResultSet &result) {
    if (!configured_) return;
    if (rx_.active() && rx_.advance(samples, level_)) character(result);
    if (rx_.inLaterStop()) { rx_.endOfRecord(samples); character(result); } // Data known, stop not seen.
    if (rx_.active()) { // The character goes on past the record.
        rx_.cancel();
        single(result, UartFragment, rx_.start(), samples, DecodeEvent::Cut);
    }
    if (inBreak_) { inBreak_ = false; single(result, UartBreak, breakStart_, samples, DecodeEvent::Open); }
    if (spaceAtStart_ && level_ == 0) {
        const bool longBreak = samples >= frame_;
        single(result, longBreak ? UartBreak : UartFragment, 0, samples, uint16_t(DecodeEvent::Cut | (longBreak ? DecodeEvent::Open : 0)));
    }
    spaceAtStart_ = false;
    closeGroup(result);
    clampToRecord(result, samples);
}

void BaudEstimator::feed(const Transition *transitions, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t index = transitions[i].index;
        if (seen_ && index > last_) {
            intervals_[next_] = index - last_;
            next_ = (next_ + 1) % Intervals;
            if (count_ < Intervals) ++count_;
        }
        last_ = index; seen_ = true;
    }
}
uint64_t BaudEstimator::pointsQ16() const {
    uint32_t shortest = 0xFFFFFFFFu;
    for (unsigned i = 0; i < count_; ++i) shortest = intervals_[i] < shortest ? intervals_[i] : shortest;
    if (count_ < 8 || shortest < 2) return 0;
    uint64_t points = 0, bits = 0;
    unsigned consistent = 0;
    for (unsigned i = 0; i < count_; ++i) {
        const uint32_t d = intervals_[i], k = (d + shortest / 2) / shortest;
        if (!k || k > 12) continue; // An idle gap, not a run of bits.
        const uint32_t ideal = k * shortest, error = d > ideal ? d - ideal : ideal - d;
        if (error * 4 > shortest) continue;
        points += d; bits += k; ++consistent;
    }
    return consistent >= 8 ? (points << 16) / bits : 0;
}
uint32_t BaudEstimator::candidateBaud(const Rational &samplesPerSecond) const {
    const uint64_t q16 = pointsQ16();
    if (!q16 || !samplesPerSecond.valid()) return 0;
    return uint32_t(((uint64_t(samplesPerSecond.numerator) << 16) + q16 / 2) / q16 / samplesPerSecond.denominator);
}

bool BaudValidator::begin(uint64_t pointsQ16, const CharacterFormat &format, bool inverted, const Rational &samplesPerSecond) {
    *this = BaudValidator{};
    format_ = format; inverted_ = inverted; fs_ = samplesPerSecond;
    bool any = false;
    for (unsigned k = 0; k < Multiples; ++k) {
        Lane &lane = lanes_[k];
        lane.clock.pointsQ16 = pointsQ16 / (k + 1);
        lane.on = format.valid() && samplesPerSecond.valid() && lane.clock.pointsQ16 >= (2ull << 16) &&
                  lane.clock.pointsQ16 <= (1ull << 40);
        any |= lane.on;
    }
    return any;
}
void BaudValidator::tally(Lane &lane) {
    const auto &rx = lane.rx;
    // A false START or a break at this rate counts against it too: a hint needs clean characters.
    if (rx.falseStart() || (rx.allSpace() && !rx.stopOk())) { if (lane.invalid < 0xFFFF) ++lane.invalid; return; }
    if (rx.parityOk() && rx.stopOk() && !lane.misaligned) { if (lane.valid < 0xFFFF) ++lane.valid; }
    else if (lane.invalid < 0xFFFF) ++lane.invalid;
}
void BaudValidator::feed(const Transition *transitions, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        const int level = (transitions[i].level ? 1 : 0) ^ int(inverted_);
        if (level_ < 0) level_ = level ^ 1;
        if (level == level_) continue;
        for (Lane &lane : lanes_) {
            if (!lane.on) continue;
            const uint32_t index = transitions[i].index;
            if (lane.rx.active() && lane.rx.advance(index, level_)) tally(lane);
            else if (level == 0 && lane.rx.inLaterStop()) { lane.rx.cutStop(index); tally(lane); }
            else if (lane.rx.active()) { // An edge inside the character: on a bit boundary at this rate?
                const uint64_t q = lane.clock.pointsQ16, phase = (uint64_t(index - lane.rx.start()) << 16) % q;
                if (phase > q / 4 && phase < q - q / 4) lane.misaligned = true;
            }
            if (level == 0 && !lane.rx.active()) { lane.rx.begin(format_, lane.clock, index); lane.misaligned = false; }
        }
        level_ = level;
    }
}
void BaudValidator::finish(uint32_t samples) {
    for (Lane &lane : lanes_)
        if (lane.on && lane.rx.active() && lane.rx.advance(samples, level_)) tally(lane);
}
BaudHint BaudValidator::result() const {
    // The best lane: a passing one before any other, then the most good characters.
    int best = -1;
    bool bestPass = false;
    unsigned passing = 0;
    for (unsigned k = 0; k < Multiples; ++k) {
        const Lane &lane = lanes_[k];
        if (!lane.on) continue;
        const bool pass = lane.valid >= MinFrames && unsigned(lane.invalid) * 8u <= lane.valid;
        passing += pass;
        if (best < 0 || (pass && !bestPass) || (pass == bestPass && lane.valid > lanes_[best].valid)) { best = int(k); bestPass = pass; }
    }
    BaudHint hint;
    if (best < 0) return hint;
    const Lane &lane = lanes_[best];
    hint.valid = lane.valid; hint.invalid = lane.invalid;
    hint.baud = uint32_t(((uint64_t(fs_.numerator) << 16) + lane.clock.pointsQ16 / 2) / lane.clock.pointsQ16 / fs_.denominator);
    hint.confirmed = bestPass && passing == 1; // Two rates both decoding cleanly: a harmonic, unsure.
    return hint;
}
} // namespace openosc::decoding::protocols
