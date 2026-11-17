#pragma once
#include "openosc/decode/protocols/types.hpp"

// Classical CAN and ISO CAN FD (ISO 11898-1:2015) on one logic line, frame by frame by the FDF bit
// (docs/SCOPE_PROTOCOL_DECODERS_CAN_FD.md): the decoder's one CAN automaton (can.hpp names it CanDecoder).
namespace openosc::decoding::protocols {
struct CanFdOptions {
    static constexpr uint32_t MaximumDataBitrate = 10000000;
    uint32_t bitrate = 500000;           // Nominal (arbitration) phase.
    uint16_t samplePointPermille = 750;  // Of the bit from its (re)synchronised start, 100..900.
    uint32_t dataBitrate = 2000000;      // Data phase of an FD frame with BRS; >= bitrate.
    uint16_t dataSamplePointPermille = 750;
    bool dominantHigh = false;           // false: RXD of a transceiver (dominant = low).
    bool valid() const {
        return bitrate && samplePointPermille >= 100 && samplePointPermille <= 900 && dataBitrate >= bitrate &&
               dataBitrate <= MaximumDataBitrate && dataSamplePointPermille >= 100 && dataSamplePointPermille <= 900;
    }
};
// How an FD frame is stored (DecodeEvent is unchanged, plan §5.5): field = ID | IDE << 29 | FDF << 31
// (bit 30 is 0: an FD frame has no RTR; a recessive RRS is an aux bit and Note); format = Bytes and
// the DLC in the meta (0 while the DLC is not received, FdDlcKnown tells); count = whole data bytes
// received; aux per the Fd* bits below; after the data bytes, when the whole CRC field was received,
// an 8-byte trailer (little-endian) with both CRCs and the stuff counts. A Classical frame is stored
// as by CanDecoder (aux: CRC-15 received | computed << 15, no trailer). CanFdView reads both.
namespace canfd {
constexpr uint32_t Fdf = 1u << 31;                 // field
constexpr uint32_t SbcReceivedMask = 0xFu;         // aux 0-3: G2 G1 G0 P as received (bit 3 = G2)
constexpr unsigned SbcCalculatedShift = 4;         // aux 4-6: own dynamic stuff count mod 8
constexpr uint32_t Brs = 1u << 7, Esi = 1u << 8, Trailer = 1u << 9, Crc21 = 1u << 10, SbcParity = 1u << 11,
                   SbcMismatch = 1u << 12, RrsRecessive = 1u << 13, ResRecessive = 1u << 14, DataPhase = 1u << 15;
constexpr unsigned StopShift = 16;                 // aux 16-19: Stop
constexpr uint32_t CrcDelimiter2 = 1u << 20, Ack2 = 1u << 21, DlcKnown = 1u << 22, SbcReceived = 1u << 23;
constexpr uint32_t ReservedMask = 0xFF000000u;     // aux 24-31: 0
// Where the frame's reception ended. Complete: EOF reached (or its last bit dominant: an overload).
enum class Stop : uint8_t { Complete, Arbitration, Control, Data, StuffCount, Crc, CrcDelimiter, AckSlot, AckDelimiter,
                            Eof, DataClock, Count };
constexpr unsigned TrailerBytes = 8, TrailerVersion = 1;
// Trailer bits: 0-20 CRC received, 21-41 CRC computed, 42-45 SBC received, 46-48 SBC computed,
// 49 CRC-21, 50-53 version, 54-63 0.
uint64_t packTrailer(uint32_t received, uint32_t computed, unsigned sbcReceived, unsigned sbcComputed, bool crc21);
inline unsigned length(unsigned dlc) { // Data bytes of a DLC (ISO 11898-1 FD; Classical caps at 8).
    static constexpr uint8_t bytes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    return bytes[dlc & 15u];
}
constexpr uint32_t Crc17Polynomial = 0x1685B, Crc21Polynomial = 0x102899, Crc17Init = 1u << 16, Crc21Init = 1u << 20;
inline unsigned gray(unsigned count) { return (count ^ (count >> 1)) & 7u; } // Reflected Gray, 3 bits.
inline unsigned fromGray(unsigned g) { g &= 7u; return g ^ (g >> 1) ^ (g >> 2); }
} // namespace canfd

// Classical and ISO CAN FD frames in one automaton. Timing: hard sync at SOF, resync on every
// recessive-to-dominant edge (no SJW limit, an analyser), each bit sampled at the sample point of its
// phase. BRS: the data phase starts at the BRS bit's sample point and ends at the CRC delimiter's
// (each next sample one whole bit of the new rate later); after an error in the data phase the
// nominal timing comes back at the error's sample point (Bosch iCC 2013). The data clock is checked
// only by an FD frame with BRS: a record too slow for it still decodes Classical and BRS = 0 frames;
// such an FD frame ends at its BRS (FsLow, NotChecked, Stop::DataClock) and the bus is searched again.
// FD CRC (ISO): CRC-17 (DLC <= 10) or CRC-21, init 1 followed by zeros, over SOF..data with the dynamic
// stuff bits and the stuff count (G2 G1 G0 P), without fixed stuff bits; a fixed stuff bit before the
// stuff count and after every fourth bit of the CRC field, each the inverse of the bit before it (else
// Framing). FD tail: a CRC delimiter of 1 or 2 recessive bits and an ACK of 1 or 2 dominant bits are
// normal (Bosch iCC 2013, NXP S32K1xx RM); Classical stays strict. Events:
//   CanFrame     SOF .. EOF (or the error point), stored as above. Flags: CheckFailed (CRC; FD also the
//                stuff count), NoAck (recessive ACK slot), Framing (form: delimiters, EOF, fixed stuff bits,
//                a recessive res), StuffError, NotChecked (CRC not reached), Note (Classical DLC > 8, FD
//                recessive RRS), Cut (frame past the record), FsLow.
//   CanErrorFlag a dominant run of >= 6 nominal bits after an error (aux: its bits; Framing above 12); Note.
//   CanOverload  a dominant in EOF's last bit or the first two intermission bits, as above.
//   CanFragment  bus activity before the record's first 11 recessive bits (Cut).
// A passive error flag (recessive) cannot be told from idle and is not reported. Non-ISO CAN FD (Bosch
// 2012) is not a supported format and is not detected: it is decoded by the ISO layout and its guards
// (FSB, stuff count and parity, the CRC init) normally fail it, but a frame that happens to pass them all
// would be shown as an ISO frame; there is no promise that every such frame is refused (§6.4).
class CanFdDecoder final : public ProtocolDecoder {
public:
    bool configure(const CanFdOptions &options, const Rational &samplesPerSecond);
    void start(ResultSet &result, int initialLevel) override;
    void feed(const Transition *transitions, unsigned count, ResultSet &result) override;
    void finish(uint32_t samples, ResultSet &result) override;
    uint32_t dropped() const { return writer_.dropped(); }
    uint32_t windowOverflows() const { return 0; } // One line: no window.
    static uint16_t crc15(uint16_t crc, int bit);  // One step of the Classical CRC-15 (0x4599).
    static uint32_t crc17(uint32_t crc, int bit);  // One step of CRC-17 (0x1685B).
    static uint32_t crc21(uint32_t crc, int bit);  // One step of CRC-21 (0x102899).

private:
    enum class State : uint8_t { Integrate, Idle, Frame };
    enum class Phase : uint8_t { Stuffed, CrcField, Tail, Intermission };
    enum class Watch : uint8_t { None, ErrorFlag, Overload };
    enum class Tail : uint8_t { Delimiter, AfterDelimiter, AckSecond, AckDelimiter, Recessive, RecessiveDominant,
                                RecessiveDominant2, Eof };
    void edge(uint32_t index, int bit, ResultSet &result);
    void sampleUntil(uint32_t end, ResultSet &result);
    void sample(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result);
    void field(int bit, uint32_t position, ResultSet &result);
    void crcField(int bit, uint32_t position, ResultSet &result);
    void tail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result);
    void classicTail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result);
    void fdTail(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result);
    void eof(int bit, uint32_t position, uint32_t bitEnd, ResultSet &result);
    void toData(uint32_t position);
    void toNominal(uint32_t position);
    void startFrame(uint32_t index);
    void emit(uint32_t last, uint16_t flags, ResultSet &result);
    void error(uint32_t position, uint16_t flags, canfd::Stop stop, ResultSet &result);
    void integrate(Watch watch, uint32_t from);
    void flag(uint32_t last, uint16_t flags, ResultSet &result);
    void flushFragment(uint32_t last, ResultSet &result);
    const BitClock &clock() const { return data_ ? dataClock_ : nominal_; }
    uint16_t samplePoint() const { return data_ ? options_.dataSamplePointPermille : options_.samplePointPermille; }

    CanFdOptions options_;
    BitClock nominal_, dataClock_;
    EventWriter writer_;
    uint32_t origin_ = 0, recessiveSince_ = 0, dominantSince_ = 0, frameFirst_ = 0, fragmentFirst_ = 0, fragmentLast_ = 0;
    uint32_t id_ = 0, crc17_ = 0, crc21_ = 0, crcReceived_ = 0, aux_ = 0;
    uint16_t crc15_ = 0, bitNo_ = 0, dlcStart_ = 0, crcStart_ = 0, k_ = 0;
    uint8_t data_bytes_[64]{};
    uint8_t run_ = 0, dlc_ = 0, bytes_ = 0, tailBit_ = 0, stuffCount_ = 0, crcBit_ = 0, sbc_ = 0;
    int8_t level_ = -1, last_ = -1; // Bit values: 0 dominant, 1 recessive.
    State state_ = State::Integrate;
    Phase phase_ = Phase::Stuffed;
    Watch watch_ = Watch::None;
    Tail tail_ = Tail::Delimiter;
    canfd::Stop stop_ = canfd::Stop::Arbitration;
    bool configured_ = false, dataValid_ = false, data_ = false, ide_ = false, rtr_ = false, fd_ = false, brs_ = false;
    bool crcDone_ = false, fragment_ = false, ack_ = false, ackSeen_ = false, noAck_ = false, fsbNext_ = false;
};
} // namespace openosc::decoding::protocols
