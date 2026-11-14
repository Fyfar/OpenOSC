#pragma once
#include "openosc/data/units.hpp"
#include <cstddef>

namespace openosc::dmm {
// Packet kinds in the factory decoder's numbering (record byte 0). DiodeWithBeeper is both the
// diode and the beeper flag (payload[9] bits 7 and 6): the factory draws the diode and speaker
// icons together and keeps the selected function. The flag is the annunciator in the packet;
// whether the beeper is enabled or sounding at that moment is not established.
enum class Kind : uint8_t { Unknown, Dc, Ac, Continuity, Capacitance, Diode, Resistance, DiodeWithBeeper };
// The physical function a kind belongs to: the diode with or without the beeper flag is one.
constexpr Kind functionKind(Kind kind) { return kind == Kind::DiodeWithBeeper ? Kind::Diode : kind; }
enum class RangeMode : uint8_t { Unknown, Auto, Manual };
struct Result {
    uint64_t timestampUs = 0;
    uint32_t sequence = 0, rawMagnitude = 0;
    NumericValue primary, secondary;
    Kind kind = Kind::Unknown;
    RangeMode range = RangeMode::Unknown;
    bool trueRms = false, packetHold = false, currentInput = false, peerRelative = false;
    uint8_t decimals = 255; // 255: no known decimal-point layout.
    char mainText[16]{}, rawUnit[12]{}, unit[12]{}, secondaryText[20]{};
};
class Decoder {
public:
    static bool decode(const uint8_t *packet, size_t bytes, Result &output);
    static char character(uint8_t segment);

private:
    static bool number(const uint8_t *payload, unsigned first, unsigned count, char *text, size_t capacity);
};
} // namespace openosc::dmm
