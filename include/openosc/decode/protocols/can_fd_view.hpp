#pragma once
#include "openosc/decode/protocols/can_fd.hpp"

namespace openosc::decoding::protocols {
// One CanFrame of a ResultSet, Classical or ISO CAN FD, read and checked in one place for the table,
// Detail and the CSV (docs/SCOPE_PROTOCOL_DECODERS_CAN_FD.md §7). of() reads nothing outside the
// event's own count + trailer bytes in the pool and refuses (false) any descriptor whose fields
// disagree: then nothing in `out` is to be shown.
struct CanFdView {
    bool fd = false, extended = false;
    bool rtr = false;           // Classical only (an FD frame has none).
    uint32_t id = 0;            // 11 or 29 bits.
    bool dlcKnown = false;      // False: the frame ended before its DLC (dlc is then 0, not "DLC 0").
    uint8_t dlc = 0;
    unsigned length = 0;        // Data bytes of the DLC (FD mapping; Classical caps at 8; RTR 0).
    unsigned count = 0;         // Data bytes received (<= length).
    const uint8_t *data = nullptr;
    // FD control bits.
    bool brs = false, esi = false, rrsRecessive = false, resRecessive = false, dataPhase = false;
    // CRC: crcBits 15 (Classical), 17 or 21 (FD) when the whole CRC field was received, else 0.
    uint8_t crcBits = 0;
    uint32_t crcReceived = 0, crcCalculated = 0;
    // FD stuff count: as received (G2 G1 G0 P), its value, its parity, the own count.
    bool sbcReceived = false, sbcParityOk = false;
    uint8_t sbcRaw = 0, sbcValue = 0, sbcCalculated = 0;
    bool crcDelimiter2 = false, ack2 = false;
    canfd::Stop stop = canfd::Stop::Complete; // FD only.
    // Hex digits a CRC of `bits` is written with, all of it: 4 (CRC-15), 5 (CRC-17), 6 (CRC-21).
    static unsigned hexDigits(unsigned bits) { return (bits + 3) / 4; }
    static bool of(const ResultSet &set, unsigned index, CanFdView &out);
};
} // namespace openosc::decoding::protocols
