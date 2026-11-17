#pragma once
#include "openosc/decode/protocols/types.hpp"

namespace openosc::decoding::protocols {
struct SpiOptions {
    enum class Align : uint8_t { Gap, Manual };
    uint8_t clockChannel = 1, dataChannel = 0; // Transition::channel of SCLK and of the data line.
    uint8_t cpol = 0, cpha = 0;                // Mode 0..3: CPOL the idle clock level, CPHA 1 samples on the trailing edge.
    uint8_t bits = 8;                          // 4..16 a word.
    bool msbFirst = true;
    Align align = Align::Gap;
    // Gap: a word starts at the first SCLK edge after a pause longer than this; Manual: 0 never splits,
    // else the same pause also ends a frame and realigns.
    uint32_t gapNs = 10000;
    uint32_t manualStart = 0;                  // Manual: the record index the first word starts at (or after).
    uint32_t windowPoints = 1;                 // As I2cOptions::windowPoints.
    bool valid() const {
        return clockChannel <= 1 && dataChannel <= 1 && clockChannel != dataChannel && cpol <= 1 && cpha <= 1 &&
               bits >= 4 && bits <= 16 && align <= Align::Manual && (align == Align::Manual || gapNs);
    }
};
// SPI on SCLK and one data line, no CS (plan §8.3): a bit is the data level at the sampling SCLK edge
// (CPOL == CPHA: rising, else falling). One SpiFrame event a burst of SCLK edges: first and last SCLK
// edge, the words in the pool (Bytes up to 8 bits, Words16 little-endian above; the bits in the format
// meta), CONT segments past 255 words; aux bits 0-4 the bits of a partial last word, 24-31 the group.
// Without CS a word boundary is only known after a pause or from the manual start: a frame without
// either before it (the record's first burst when the record starts inside it) is Ambiguous. A partial
// last word -> Cut (not stored); SCLK not at CPOL before a frame -> Framing; a data edge within the window
// of a sampling edge -> Ambiguous; a frame that may go on past the record -> Open. FsLow: a whole SCLK half
// period inside the frame (between two of its edges) under ceil(MinPointsPerBit / 2) = 3 points.
class SpiDecoder final : public ProtocolDecoder {
public:
    bool configure(const SpiOptions &options, const Rational &samplesPerSecond);
    void start(ResultSet &result, int initialLevel) override; // initialLevel: CH1, as I2cDecoder.
    void initialLevel(uint8_t channel, int level);
    void feed(const Transition *transitions, unsigned count, ResultSet &result) override;
    void finish(uint32_t samples, ResultSet &result) override;
    uint32_t dropped() const { return writer_.dropped(); }
    uint32_t windowOverflows() const { return queue_.overflows(); } // Bursts past the window (OVR).
private:
    void process(const CoincidenceQueue::Item &item, ResultSet &result);
    void clock(const Transition &t, bool ambiguous, ResultSet &result);
    void open(uint32_t index, bool aligned, bool idleOk, ResultSet &result);
    void close(uint32_t last, uint16_t flags, ResultSet &result);
    SpiOptions options_;
    CoincidenceQueue queue_;
    EventWriter writer_;
    uint32_t gap_ = 0, lastEdge_ = 0, wordFirst_ = 0;
    uint16_t word_ = 0;
    uint8_t bits_ = 0;
    int8_t clock_ = -1, data_ = -1;
    bool configured_ = false, seenEdge_ = false, frame_ = false, manualDone_ = false;
};
} // namespace openosc::decoding::protocols
