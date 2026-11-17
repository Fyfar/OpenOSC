#include "openosc/decode/protocols/can_fd_view.hpp"

namespace openosc::decoding::protocols {
namespace {
using namespace canfd;
bool inPool(const DecodeEvent &e, unsigned bytes) {
    if (!bytes) return true;
    return e.payload != 0xFFFF && unsigned(e.payload) + bytes <= ResultSet::Pool;
}
bool classic(const ResultSet &set, const DecodeEvent &e, CanFdView &v) {
    const unsigned width = formatWidth(e.format);
    if (e.field & (1u << 31) || (width != DecodeEvent::None && width != DecodeEvent::Bytes) || e.count > 8 ||
        (e.count && width != DecodeEvent::Bytes) || !inPool(e, e.count) || (e.aux >> 30))
        return false;
    v.extended = (e.field >> 29) & 1u;
    v.rtr = (e.field >> 30) & 1u;
    v.id = e.field & (v.extended ? 0x1FFFFFFFu : 0x7FFu);
    if (!v.extended && (e.field & 0x1FFFF800u)) return false;
    const bool checked = !(e.flags & NotChecked);
    v.dlc = uint8_t(formatMeta(e.format) & 15u);
    v.dlcKnown = checked || v.dlc; // A frame cut before its DLC also says 0 (CanDecoder).
    v.length = v.rtr ? 0u : (v.dlc > 8 ? 8u : v.dlc);
    v.count = e.count;
    if (v.dlcKnown && v.count > v.length) return false;
    v.data = e.count ? set.pool(e.payload) : nullptr;
    if (checked) {
        v.crcBits = 15;
        v.crcReceived = e.aux & 0x7FFFu;
        v.crcCalculated = (e.aux >> 15) & 0x7FFFu;
    } else if (e.aux) {
        return false; // No CRC field received: nothing in aux.
    }
    return true;
}
} // namespace

bool CanFdView::of(const ResultSet &set, unsigned index, CanFdView &out) {
    out = CanFdView{};
    if (index >= set.count()) return false;
    const DecodeEvent &e = set.event(index);
    if (e.kind != CanFrame) return false;
    CanFdView v;
    if (!(e.field & Fdf)) {
        if (!classic(set, e, v)) return false;
        out = v;
        return true;
    }
    v.fd = true;
    const uint32_t aux = e.aux;
    const unsigned width = formatWidth(e.format);
    const bool trailer = aux & Trailer;
    if ((e.field & (1u << 30)) || (aux & ReservedMask) || ((aux >> StopShift) & 15u) >= unsigned(Stop::Count)) return false;
    v.extended = (e.field >> 29) & 1u;
    v.id = e.field & (v.extended ? 0x1FFFFFFFu : 0x7FFu);
    if (!v.extended && (e.field & 0x1FFFF800u)) return false;
    v.dlcKnown = aux & DlcKnown;
    v.dlc = uint8_t(formatMeta(e.format) & 31u);
    if (v.dlc > 15) return false;
    v.length = v.dlcKnown ? canfd::length(v.dlc) : 0u;
    v.count = e.count;
    // The DLC and what follows from it: no DLC, no data, no CRC field.
    if (!v.dlcKnown && (v.dlc || v.count || trailer || (aux & (Crc21 | SbcReceived)))) return false;
    if (v.dlcKnown && (v.count > v.length || bool(aux & Crc21) != (v.dlc > 10))) return false;
    if ((v.count || trailer) ? width != DecodeEvent::Bytes : (width != DecodeEvent::None && width != DecodeEvent::Bytes))
        return false;
    if (!inPool(e, v.count + (trailer ? TrailerBytes : 0u))) return false;
    v.brs = aux & Brs; v.esi = aux & Esi; v.rrsRecessive = aux & RrsRecessive; v.resRecessive = aux & ResRecessive;
    v.dataPhase = aux & DataPhase;
    if (v.dataPhase && !v.brs) return false;
    v.crcDelimiter2 = aux & CrcDelimiter2; v.ack2 = aux & Ack2;
    v.stop = Stop((aux >> StopShift) & 15u);
    v.sbcReceived = aux & SbcReceived;
    if (!v.sbcReceived && (aux & SbcReceivedMask)) return false;
    v.sbcRaw = uint8_t(aux & SbcReceivedMask);
    v.sbcCalculated = uint8_t((aux >> SbcCalculatedShift) & 7u);
    v.sbcValue = uint8_t(fromGray(v.sbcRaw >> 1));
    const unsigned g = v.sbcRaw >> 1;
    v.sbcParityOk = (v.sbcRaw & 1u) == (((g >> 2) ^ (g >> 1) ^ g) & 1u);
    v.data = v.count ? set.pool(e.payload) : nullptr;
    if (!trailer) {
        // No CRC field: no verdict of its own (NotChecked) and no stuff count verdict.
        if (!(e.flags & NotChecked) || (aux & (SbcParity | SbcMismatch))) return false;
        out = v;
        return true;
    }
    // The whole CRC field: every data byte and the stuff count before it, both CRCs in the trailer.
    if ((e.flags & NotChecked) || !v.sbcReceived || v.count != v.length) return false;
    const uint8_t *t = set.pool(uint16_t(e.payload + v.count));
    uint64_t packed = 0;
    for (unsigned i = 0; i < TrailerBytes; ++i) packed |= uint64_t(t[i]) << (8 * i);
    const bool wide = aux & Crc21;
    const unsigned bits = wide ? 21u : 17u;
    const uint32_t received = uint32_t(packed & 0x1FFFFF), computed = uint32_t((packed >> 21) & 0x1FFFFF);
    if (((packed >> 50) & 15u) != TrailerVersion || (packed >> 54) || bool((packed >> 49) & 1u) != wide ||
        ((packed >> 42) & 15u) != v.sbcRaw || ((packed >> 46) & 7u) != v.sbcCalculated ||
        (received >> bits) || (computed >> bits))
        return false;
    // The verdict bits must say what the trailer shows.
    if (bool(aux & SbcParity) == v.sbcParityOk || bool(aux & SbcMismatch) == (v.sbcValue == v.sbcCalculated)) return false;
    if (received != computed || (aux & (SbcParity | SbcMismatch))) { if (!(e.flags & CheckFailed)) return false; }
    else if (e.flags & CheckFailed) return false;
    v.crcBits = uint8_t(bits);
    v.crcReceived = received;
    v.crcCalculated = computed;
    out = v;
    return true;
}
} // namespace openosc::decoding::protocols
