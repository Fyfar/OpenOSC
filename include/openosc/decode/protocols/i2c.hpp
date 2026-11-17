#pragma once
#include "openosc/decode/protocols/types.hpp"

namespace openosc::decoding::protocols {
struct I2cOptions {
    uint8_t sdaChannel = 0, sclChannel = 1; // Transition::channel of each line (0 CH1, 1 CH2).
    // Edges of SDA and SCL closer than this many points are of unknown order (plan §6.2: one point plus
    // the lanes' skew in points of the record): such a bit or condition is flagged Ambiguous.
    uint32_t windowPoints = 1;
    // A low SCL longer than this many times the shortest high SCL of the transaction is stretching.
    uint8_t stretchRatio = 4;
    bool valid() const { return sdaChannel <= 1 && sclChannel <= 1 && sdaChannel != sclChannel && stretchRatio >= 2; }
};
// I2C (NXP UM10204, plan §8.2): START / Sr / STOP are SDA edges while SCL is high, a bit is SDA at the
// rising SCL edge, nine bits a byte (the ninth ACK = 0, NACK = 1). One I2cTransaction event from its
// START or Sr to its Sr or STOP:
//   field  7-bit address, or the 10-bit one (11110xx + second byte)
//   aux    bit 0 read, 1 ten-bit, 2 address NACK, 3 began with Sr, 4 ended with Sr, 5 ended with STOP,
//          bits 8-15 index of the first NACKed data byte (0xFF none), 16-23 NACKed data bytes (saturated),
//          24-31 segment group
//   payload the data bytes (Bytes); more than 255 continue in CONT segments of the group.
// Flags: NoAck for a NACKed address or written byte, or a read NACK followed by another byte; Framing
// for a START/STOP inside a byte; Ambiguous for SDA and SCL edges in one window (such an SDA edge is a
// data change, never START/STOP, and a bit sampled there takes SDA after it); Note for stretching;
// Cut when the transaction runs past the record. Clocks before the record's first START give one Cut
// I2cFragment. A 10-bit read after Sr takes the address of the 10-bit write before it, else Ambiguous.
// FsLow (transaction or fragment): a whole SCL half period inside it under ceil(MinPointsPerBit / 2) = 3 points.
class I2cDecoder final : public ProtocolDecoder {
public:
    bool configure(const I2cOptions &options);
    // initialLevel: CH1's level (-1 unknown); initialLevel(channel, level) sets either line.
    void start(ResultSet &result, int initialLevel) override;
    void initialLevel(uint8_t channel, int level);
    void feed(const Transition *transitions, unsigned count, ResultSet &result) override;
    void finish(uint32_t samples, ResultSet &result) override;
    uint32_t dropped() const { return writer_.dropped(); }
    uint32_t windowOverflows() const { return queue_.overflows(); } // Bursts past the window (OVR).
private:
    enum class Phase : uint8_t { Idle, Address, AddressLow, Data };
    void process(const CoincidenceQueue::Item &item, ResultSet &result);
    void clock(const Transition &t, bool ambiguous, ResultSet &result);
    void data(const Transition &t, bool ambiguous, ResultSet &result, bool defer = true);
    void replayEarly(ResultSet &result);
    void condition(uint32_t index, bool start, ResultSet &result);
    void begin(uint32_t index, bool repeated, ResultSet &result);
    void close(uint32_t index, uint32_t endBits, ResultSet &result);
    void byte(uint32_t index, ResultSet &result);
    void flushFragment(uint32_t index, ResultSet &result);
    void mark(uint16_t flags, ResultSet &result);
    void lowRate(ResultSet &result);
    I2cOptions options_;
    CoincidenceQueue queue_;
    // SDA edges before SCL's first edge when SCL's level was not given: replayed once that edge tells it
    // (SCL held one level until then).
    CoincidenceQueue::Item early_[4]{};
    uint8_t earlyCount_ = 0;
    EventWriter writer_;
    uint32_t sclRise_ = 0, sclFall_ = 0, minHigh_ = 0, byteFirst_ = 0, fragmentFirst_ = 0, fragmentLast_ = 0;
    uint16_t lastTenBit_ = 0xFFFF; // The 10-bit address written before an Sr, for a 10-bit read.
    uint16_t shift_ = 0, dataBytes_ = 0;
    uint8_t bits_ = 0, nacked_ = 0, firstNack_ = 0xFF;
    int8_t sda_ = -1, scl_ = -1;
    Phase phase_ = Phase::Idle;
    bool configured_ = false, fragment_ = false, fragmentAmbiguous_ = false, readNack_ = false, seenFall_ = false,
         seenCondition_ = false;
    // The unit's own SCL edges (a whole half period inside it), and a low one in a fragment: Unit* bits.
    uint8_t unit_ = 0;
};
} // namespace openosc::decoding::protocols
