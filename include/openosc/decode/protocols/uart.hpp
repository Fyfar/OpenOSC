#pragma once
#include "openosc/decode/protocols/types.hpp"

namespace openosc::decoding::protocols {
enum class Parity : uint8_t { None, Even, Odd, Mark, Space };
// One asynchronous character: START, 5..9 data bits, an optional parity bit, 1, 1.5 or 2 stop bits.
struct CharacterFormat {
    uint8_t dataBits = 8;      // 5..9
    Parity parity = Parity::None;
    uint8_t stopHalfBits = 2;  // 2, 3 or 4: 1, 1.5 or 2 stop bits.
    bool msbFirst = false;
    bool valid() const { return dataBits >= 5 && dataBits <= 9 && stopHalfBits >= 2 && stopHalfBits <= 4 && parity <= Parity::Space; }
    uint32_t milliBits() const { // START to the end of the last stop bit.
        return (1u + dataBits + (parity != Parity::None)) * 1000u + stopHalfBits * 500u;
    }
};
// Samples one character from the falling edge of its START: every bit at its centre counted from that
// edge by the bit clock, no resynchronisation inside the character (plan §6.3). The whole configured stop
// is checked (an analyser, not a receiving chip): the first stop bit at its centre, 1.5 stop bits again
// at the centre of the half bit, 2 at the centre of the second. A START edge before that later sample
// (cutStop) is a stop too short: a framing error, the character ending at that edge. The caller hands it the
// line level piecewise: advance(end, level) samples every pending point before `end` at `level`.
class CharacterReceiver {
public:
    void begin(const CharacterFormat &format, const BitClock &clock, uint32_t start);
    // True once the character is complete (its last stop sample taken, or a false START).
    bool advance(uint32_t end, int level);
    bool active() const { return active_; }
    void cancel() { active_ = false; }
    // Only the later stop sample (1.5/2 stop bits) is still due: an edge to space now is an early START.
    bool inLaterStop() const;
    void cutStop(uint32_t at);
    // The record ends before the later stop sample: the character is known, its stop not verified.
    void endOfRecord(uint32_t at) { cut_ = at; cutShort_ = true; incomplete_ = true; active_ = false; }
    bool incomplete() const { return incomplete_; }
    uint32_t start() const { return start_; }
    // The configured end of the last stop bit, or the early START that cut it.
    uint32_t end() const { return cutShort_ ? cut_ : start_ + clock_.points(format_.milliBits()); }
    bool stopCut() const { return cutShort_; }
    // An early START cut the stop after its first stop bit was sampled at mark.
    bool firstStopOk() const { return cutShort_ ? cutFirstStopOk_ : stopOk_; }
    // Results of a complete character.
    uint16_t value() const { return value_; }
    bool falseStart() const { return falseStart_; }
    bool parityOk() const { return parityOk_; }
    bool stopOk() const { return stopOk_; }
    bool allSpace() const { return allSpace_; } // Every sampled bit 0, the stop bit included (a break).
private:
    uint32_t position(unsigned step) const;
    void sample(unsigned step, int bit);
    CharacterFormat format_;
    BitClock clock_;
    uint32_t start_ = 0, cut_ = 0;
    uint16_t value_ = 0;
    uint8_t step_ = 0, steps_ = 0, stopStep_ = 0, ones_ = 0;
    bool active_ = false, falseStart_ = false, cutShort_ = false, incomplete_ = false, parityOk_ = true, stopOk_ = true, allSpace_ = true;
    bool cutFirstStopOk_ = false;
};

struct UartOptions {
    uint32_t baud = 115200;
    CharacterFormat format;
    bool inverted = false;       // The line idles low (a level shifter without inversion back).
    // Characters closer than this many bit times (end of one to START of the next) form one event; 0: one
    // event a character. An error, a break, an unsure START or 255 elements always end the group.
    uint16_t groupIdleBits = 20;
    // The capture's edge timing uncertainty in points (four-lane read skew, ProtocolMachine::window - 1;
    // 0: exact). When half a bit is not longer, a START may be timed off by half a bit: every character
    // is then Ambiguous (a quality flag only: the phase and the values are unchanged).
    uint32_t timingUncertaintyPoints = 0;
    bool valid() const { return baud && format.valid(); }
};
// UART on one line (plan §8.1). Events: UartData (Bytes for 5-8 bits, Words16 little-endian for 9; the
// bits in the format meta), UartBreak, UartFragment. A START is the first idle-to-space edge. The phase is
// known only after a whole frame of idle before a START or a complete break; until then every character
// is Ambiguous (grouped as such), and a right stop bit keeps a known phase but never makes one (a record
// cut into a stream may meet a stop by chance). Parity -> ParityError,
// a stop sample at space or a START before the configured stop's end -> Framing (that character then
// ends at the START); space through the stop bits -> a break (to the edge back to idle).
class UartDecoder final : public ProtocolDecoder {
public:
    // False (and nothing decoded) for invalid options or a bit outside 2..2^24 points.
    bool configure(const UartOptions &options, const Rational &samplesPerSecond);
    void start(ResultSet &result, int initialLevel) override;
    void feed(const Transition *transitions, unsigned count, ResultSet &result) override;
    void finish(uint32_t samples, ResultSet &result) override;
    uint32_t dropped() const { return writer_.dropped(); } // Units lost for the pool (EVT> with lost()).
    uint32_t falseStarts() const { return falseStarts_; }
private:
    void edge(uint32_t index, int level, ResultSet &result);
    void character(ResultSet &result);
    void closeGroup(ResultSet &result);
    void holdTo(uint32_t start, ResultSet &result);
    void single(ResultSet &result, uint8_t kind, uint32_t first, uint32_t last, uint16_t flags);
    void headerAt(ResultSet &result, uint32_t first, uint16_t flags);
    UartOptions options_;
    BitClock clock_;
    CharacterReceiver rx_;
    EventWriter writer_;
    uint32_t frame_ = 0, groupGap_ = 0; // Points of a whole frame, of the grouping gap.
    uint32_t idleSince_ = 0, spaceSince_ = 0, groupEnd_ = 0, breakStart_ = 0, falseStarts_ = 0;
    int level_ = -1;                    // Logical: 1 mark (idle), 0 space; -1 not known yet.
    uint16_t groupFlags_ = 0;           // The open group's flags: 0, or Ambiguous.
    bool configured_ = false, synced_ = false, unsure_ = false, inBreak_ = false, spaceAtStart_ = false, group_ = false;
    bool timingUnsure_ = false;         // Half a bit within the capture's edge uncertainty (configure).
};

// Autobaud as a hint only (plan §6.3), in two steps; neither ever changes UartOptions::baud.
// 1. BaudEstimator: a candidate bit time from the intervals between edges of several frames, each an
//    integer multiple of the shortest one. Not to be shown by itself: the shortest interval may be 2 or 3
//    bits (a harmonic) or a glitch.
// 2. BaudValidator: decodes the same transitions at the candidate, half and a third of its bit time in
//    the configured format; a character is correct when its parity and whole stop are right and every edge
//    inside it lies within a quarter bit of a bit boundary from its START; a false START or a break counts
//    as a bad one. A hint is confirmed only when
//    exactly one rate gives at least MinFrames correct characters and at most one bad one per eight good.
class BaudEstimator {
public:
    static constexpr unsigned Intervals = 32;
    void reset() { *this = BaudEstimator{}; }
    void feed(const Transition *transitions, unsigned count);
    // Points a bit in Q16 (0: fewer than 8 consistent intervals), and that candidate's baud at Fs:
    // unconfirmed (BaudValidator decides), never to be shown or applied on its own.
    uint64_t pointsQ16() const;
    uint32_t candidateBaud(const Rational &samplesPerSecond) const;
private:
    uint32_t intervals_[Intervals]{};
    uint32_t last_ = 0;
    unsigned count_ = 0, next_ = 0;
    bool seen_ = false;
};
struct BaudHint {
    uint32_t baud = 0;             // Of the best lane (0: none decoded anything).
    uint16_t valid = 0, invalid = 0;
    bool confirmed = false;        // Only then may the UI offer it (as a hint).
};
class BaudValidator {
public:
    static constexpr unsigned Multiples = 3, MinFrames = 3;
    // False when no lane has at least 2 points a bit.
    bool begin(uint64_t pointsQ16, const CharacterFormat &format, bool inverted, const Rational &samplesPerSecond);
    void feed(const Transition *transitions, unsigned count);
    void finish(uint32_t samples);
    BaudHint result() const;
private:
    struct Lane {
        BitClock clock;
        CharacterReceiver rx;
        uint16_t valid = 0, invalid = 0;
        bool on = false, misaligned = false; // An edge of the current character off its bit grid.
    };
    static void tally(Lane &lane);
    Lane lanes_[Multiples];
    CharacterFormat format_;
    Rational fs_;
    int level_ = -1;
    bool inverted_ = false;
};
} // namespace openosc::decoding::protocols
