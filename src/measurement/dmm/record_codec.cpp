#include "openosc/measurement/dmm/record_codec.hpp"
#include "openosc/data/binary.hpp"
#include <cstring>

namespace openosc::dmm {
namespace {
bool copy(const char *input, size_t limit, char *output, size_t capacity) {
    size_t n = 0;
    while (n < limit && input[n])
        ++n;
    if (n == limit || n >= capacity)
        return false;
    std::memcpy(output, input, n + 1);
    return true;
}
} // namespace
bool RecordCodec::encode(const Result &r, uint8_t *output, size_t capacity) {
    if (!output || capacity < Bytes || unsigned(r.kind) > 7 || unsigned(r.range) > 2)
        return false;
    uint8_t bytes[Bytes]{};
    if (!copy(r.mainText, sizeof r.mainText, reinterpret_cast<char *>(bytes + 0x11), 16) ||
        !copy(r.rawUnit, sizeof r.rawUnit, reinterpret_cast<char *>(bytes + 0x39), 16) ||
        !copy(r.secondaryText, sizeof r.secondaryText, reinterpret_cast<char *>(bytes + 0x49), 16))
        return false;
    bytes[0] = static_cast<uint8_t>(r.kind);
    std::strcpy(reinterpret_cast<char *>(bytes + 0x21), r.range == RangeMode::Auto     ? "AUTO"
                                                        : r.range == RangeMode::Manual ? "MANU"
                                                                                       : "");
    if (r.trueRms)
        std::memcpy(bytes + 0x29, "TRUE RMS", 9);
    bytes[0x59] = r.mainText[0] == '-';
    bytes[0x5a] = r.currentInput;
    Binary::writeLe(bytes + 0x60, r.rawMagnitude, 4);
    bytes[0x65] = bytes[0x66] = r.packetHold;
    bytes[0xaf] = r.range == RangeMode::Manual;
    bytes[0xac] = r.peerRelative;
    bytes[0xad] = r.decimals;
    std::memcpy(output, bytes, Bytes);
    return true;
}
bool RecordCodec::decode(const uint8_t *in, size_t bytes, Result &output) {
    if (!in || bytes != Bytes || in[0] > 7 || in[0x65] > 1 || in[0x66] > 1 || in[0x5a] > 1)
        return false;
    Result result;
    char range[8], rms[16];
    if (!copy(reinterpret_cast<const char *>(in + 0x11), 16, result.mainText, sizeof result.mainText) ||
        !copy(reinterpret_cast<const char *>(in + 0x39), 16, result.rawUnit, sizeof result.rawUnit) ||
        !copy(reinterpret_cast<const char *>(in + 0x49), 16, result.secondaryText,
              sizeof result.secondaryText) ||
        !copy(reinterpret_cast<const char *>(in + 0x21), 8, range, sizeof range) ||
        !copy(reinterpret_cast<const char *>(in + 0x29), 16, rms, sizeof rms))
        return false;
    result.kind = static_cast<Kind>(in[0]);
    result.packetHold = in[0x66];
    result.peerRelative = in[0xac] != 0;
    result.decimals = in[0xad] <= 4 ? in[0xad] : 255;
    result.currentInput = in[0x5a];
    result.trueRms = !std::strcmp(rms, "TRUE RMS");
    result.range = !std::strcmp(range, "AUTO")   ? RangeMode::Auto
                   : !std::strcmp(range, "MANU") ? RangeMode::Manual
                                                 : RangeMode::Unknown;
    result.rawMagnitude = Binary::readLe(in + 0x60, 4);
    std::strcpy(result.unit, result.rawUnit);
    if (!std::strcmp(result.unit, "&"))
        std::strcpy(result.unit, "Ohm");
    if (!std::strcmp(result.unit, "K&"))
        std::strcpy(result.unit, "kOhm");
    if (!std::strcmp(result.unit, "M&"))
        std::strcpy(result.unit, "MOhm");
    NumericValue unitType;
    if (UnitParser::parse("0", result.rawUnit, unitType))
        result.primary.quantity = unitType.quantity;
    if (std::strchr(result.mainText, 'L'))
        result.primary.status = DataStatus::Overload;
    else if (!UnitParser::parse(result.mainText, result.rawUnit, result.primary))
        result.primary.status = DataStatus::Unsupported;
    if (result.trueRms) {
        char number[sizeof result.secondaryText];
        std::strcpy(number, result.secondaryText);
        char *unit = std::strstr(number, "KHz");
        const char *suffix = "KHz";
        if (!unit) {
            unit = std::strstr(number, "Hz");
            suffix = "Hz";
        }
        if (unit) {
            *unit = 0;
            if (!UnitParser::parse(number, suffix, result.secondary))
                result.secondary.status = DataStatus::Unsupported;
        }
    }
    output = result;
    return true;
}
bool RecordCodec::encodeHold(const Result &result, uint8_t *output, size_t capacity) {
    if (!output || capacity < HoldBytes)
        return false;
    uint8_t bytes[HoldBytes]{};
    bytes[0] = 0xaa;
    bytes[HoldBytes - 1] = 0xee;
    if (!encode(result, bytes + 1, Bytes))
        return false;
    std::memcpy(output, bytes, HoldBytes);
    return true;
}
bool RecordCodec::decodeHold(const uint8_t *input, size_t bytes, Result &output) {
    return input && bytes == HoldBytes && input[0] == 0xaa && input[HoldBytes - 1] == 0xee &&
           decode(input + 1, Bytes, output);
}
} // namespace openosc::dmm
