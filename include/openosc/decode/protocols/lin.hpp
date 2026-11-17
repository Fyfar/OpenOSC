#pragma once
#include "openosc/decode/protocols/uart.hpp"

namespace openosc::decoding::protocols {
struct LinOptions {
    enum class Checksum : uint8_t { Classic, Enhanced };
    uint32_t baud = 19200;           // Nominal: the break threshold and the first guess; SYNC refines it.
    // Dominant at least this many nominal bits is a break (10..40). 11: the receiver's threshold of LIN 2.2A
    // §2.3.1.1 (a master sends at least 13); a 0x00 with a framing error (10 bits) stays no break.
    uint8_t breakBits = 11;
    Checksum checksum = Checksum::Enhanced; // IDs 0x3C/0x3D always use classic (LIN 2.x diagnostics).
    uint16_t syncTolerancePermille = 150; // The SYNC bit time may differ this much from the nominal one.
    // Data bytes of each frame ID (from the LDF), 0 = unknown: the response is then kept raw to the next
    // break (LEN?, checksum NOT CHECKED). The diagnostic frames 0x3C/0x3D are 8 bytes.
    uint8_t lengths[64] = {};
    LinOptions() { lengths[0x3C] = lengths[0x3D] = 8; }
    bool valid() const;
};
// LIN (ISO 17987 / LIN 2.2A, plan §8.4) on one line (RXD: dominant = low). One LinFrame event from the
// break's first point to the checksum's stop bit:
//   field  the PID byte as received (ID in bits 0-5, P0/P1 in 6-7); 0 while the header is incomplete
//   aux    bits 0-7 the received checksum, 8-15 the computed one, bit 16 enhanced model used, bit 17 the
//          length came from the table, bits 24-31 the segment group
//   payload the data bytes (without the checksum); with an unknown length every byte after the PID
// SYNC must give alternating edges at bit boundaries of one bit time within the tolerance, else
// SyncError; that bit time then samples the rest of the frame. Flags: ParityError (PID parity, then length
// unknown), CheckFailed, NotChecked, LengthUnknown, NoAck (header without response), Framing (stop bit,
// short response, short delimiter, a header ended by a break), Cut (frame past the record), Open (unknown
// length at the record's end). Every frame whose checksum was not compared carries NotChecked (a bad or
// partial SYNC, no or partial PID, short response, missing checksum, a cut or a break at the record's end);
// CheckFailed only when the checksum byte was received and compared.
// Activity before the first break: a Cut LinFragment; bytes between frames: a Framing LinFragment.
class LinDecoder final : public ProtocolDecoder {
public:
    bool configure(const LinOptions &options, const Rational &samplesPerSecond);
    void start(ResultSet &result, int initialLevel) override;
    void feed(const Transition *transitions, unsigned count, ResultSet &result) override;
    void finish(uint32_t samples, ResultSet &result) override;
    uint32_t dropped() const { return writer_.dropped(); }
    static uint8_t protectedId(uint8_t id); // ID with its parity bits P0, P1.
    static uint8_t checksum(const uint8_t *data, unsigned count, uint8_t seed); // seed: PID (enhanced) or 0.
private:
    enum class State : uint8_t { Hunt, Delimiter, Sync, Bytes, Between };
    void edge(uint32_t index, int level, ResultSet &result);
    void onBreak(uint32_t first, uint32_t end, ResultSet &result);
    void sync(uint32_t index, ResultSet &result);
    void character(ResultSet &result);
    void store(uint8_t value, bool framing, uint32_t first, uint32_t last, ResultSet &result);
    void closeFrame(uint32_t last, uint16_t flags, ResultSet &result);
    void flushFragment(uint32_t end, ResultSet &result, bool cut = false); // cut: still at space at the record end.
    LinOptions options_;
    BitClock nominal_, clock_;
    CharacterReceiver rx_;
    EventWriter writer_;
    CharacterFormat format_; // 8N1, LSB first.
    uint32_t breakPoints_ = 0, spaceSince_ = 0, breakEnd_ = 0, lastEnd_ = 0, fragmentFirst_ = 0;
    uint32_t syncEdges_[10]{};
    uint16_t sum_ = 0;
    uint8_t syncCount_ = 0, pid_ = 0, length_ = 0, data_ = 0;
    int8_t level_ = -1;
    State state_ = State::Hunt;
    bool configured_ = false, frame_ = false, pidSeen_ = false, enhanced_ = false, pendingSpace_ = false,
         fragment_ = false, syncChecked_ = false;
};
} // namespace openosc::decoding::protocols
