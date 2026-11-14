#include "openosc/measurement/dmm/packet.hpp"
#include "openosc/measurement/dmm/result.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::dmm {
char Decoder::character(uint8_t segment) {
    static constexpr uint8_t patterns[] = {0,    0x0a, 0x24, 0x2f, 0x4e, 0x61, 0x67, 0x8a, 0x8f, 0xad,
                                           0xc7, 0xcf, 0xe1, 0xe4, 0xe5, 0xe7, 0xeb, 0xee, 0xef};
    static constexpr char chars[] = " 1-d4Lb73259cfe60a8";
    for (unsigned i = 0; i < sizeof patterns; ++i)
        if (segment == patterns[i])
            return chars[i];
    return '?';
}
bool Decoder::number(const uint8_t *p, unsigned first, unsigned count, char *text, size_t capacity) {
    size_t written = 0;
    if (p[first] & 0x10)
        text[written++] = '-';
    for (unsigned i = 0; i < count; ++i) {
        uint8_t segment = p[first - i];
        if (i && (segment & 0x10)) {
            if (written + 1 >= capacity)
                return false;
            text[written++] = '.';
        }
        char c = character(segment & 0xef);
        if (c != ' ') {
            if (written + 1 >= capacity)
                return false;
            text[written++] = c;
        }
    }
    text[written] = 0;
    return true;
}
bool Decoder::decode(const uint8_t *packet, size_t bytes, Result &output) {
    if (!Packet::valid(packet, bytes))
        return false;
    const auto *p = packet + 1;
    Result candidate;
    unsigned points = 0;
    for (unsigned i = 0; i < 4; ++i) if (p[i] & 0x10) { candidate.decimals = i + 1; ++points; }
    if (points > 1) candidate.decimals = 255;
    if (!number(p, 4, 5, candidate.mainText, sizeof candidate.mainText))
        return false;
    candidate.kind = (p[10] & 0x10)          ? Kind::Dc
                     : (p[10] & 0x40)        ? Kind::Ac
                     : (p[15] & 4)           ? Kind::Capacitance
                     : (p[9] & 0xc0) == 0xc0 ? Kind::DiodeWithBeeper
                     : (p[9] & 0x80)         ? Kind::Diode
                     : (p[9] & 0x40)         ? Kind::Continuity
                     : (p[15] & 0x40)        ? Kind::Resistance
                                             : Kind::Unknown;
    candidate.peerRelative = p[9] & 0x20;
    candidate.trueRms = p[10] & 0x40;
    candidate.packetHold = p[10] & 0x80;
    candidate.currentInput = p[15] & 1;
    struct Flag {
        unsigned byte;
        uint8_t mask;
        char character;
    };
    static constexpr Flag flags[] = {{14, 64, 'n'}, {15, 8, 'm'}, {14, 128, 'u'}, {15, 4, 'F'},
                                     {15, 2, 'V'},  {15, 1, 'A'}, {15, 16, 'M'},  {15, 32, 'K'},
                                     {15, 64, '&'}, {14, 16, 'x'}};
    unsigned length = 0;
    for (auto f : flags)
        if (p[f.byte] & f.mask)
            candidate.rawUnit[length++] = f.character;
    std::strcpy(candidate.unit, candidate.rawUnit);
    if (!std::strcmp(candidate.rawUnit, "&") || !std::strcmp(candidate.rawUnit, "K&") ||
        !std::strcmp(candidate.rawUnit, "M&")) {
        const char *prefix = !std::strcmp(candidate.rawUnit, "K&")   ? "k"
                             : !std::strcmp(candidate.rawUnit, "M&") ? "M"
                                                                     : "";
        std::snprintf(candidate.unit, sizeof candidate.unit, "%sOhm", prefix);
    }
    bool rawDigits = true;
    for (unsigned i = 0; i < 5; ++i) {
        const char c = character(p[4 - i] & 0xef);
        if (c >= '0' && c <= '9')
            candidate.rawMagnitude = candidate.rawMagnitude * 10 + unsigned(c - '0');
        else if (c != ' ')
            rawDigits = false;
    }
    const bool overload = std::strchr(candidate.mainText, 'L');
    NumericValue unitType;
    if (UnitParser::parse("0", candidate.rawUnit, unitType))
        candidate.primary.quantity = unitType.quantity;
    if (overload) {
        candidate.rawMagnitude = 66000;
        candidate.primary.status = DataStatus::Overload;
    } else if (!rawDigits || !UnitParser::parse(candidate.mainText, candidate.rawUnit, candidate.primary))
        candidate.primary.status = DataStatus::Unsupported;
    if (candidate.trueRms) {
        char numberText[16];
        if (!number(p, 8, 4, numberText, sizeof numberText))
            return false;
        const char *unit = p[9] & 1 ? "KHz" : "Hz";
        std::snprintf(candidate.secondaryText, sizeof candidate.secondaryText, "%s%s", numberText, unit);
        if (!UnitParser::parse(numberText, unit, candidate.secondary))
            candidate.secondary.status = DataStatus::Unsupported;
    }
    // Range: AUTO payload[10].2 first, then MANU .3, else Unknown, as the factory (zt_fw multimeter.md,
    // record +21). One compatibility difference: this controller's manual V/A/Ohm ranges come with
    // neither bit (packets captured after RANGE, functionality.cpp; HIL VDC 1-4, RES 1-5), so those
    // three quantities without a bit stay Manual. Not capacitance (no captured manual Cap packet),
    // diode or continuity: there no bit means Unknown (no range keys, mirror or restore).
    const auto quantity = candidate.primary.quantity;
    const bool ranged = (quantity == Quantity::Voltage || quantity == Quantity::Current || quantity == Quantity::Resistance) &&
                        candidate.kind != Kind::Diode && candidate.kind != Kind::DiodeWithBeeper &&
                        candidate.kind != Kind::Continuity && candidate.kind != Kind::Capacitance;
    candidate.range = p[10] & 4 ? RangeMode::Auto : (p[10] & 8) || ranged ? RangeMode::Manual : RangeMode::Unknown;
    output = candidate;
    return true;
}
} // namespace openosc::dmm
