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
    // A digit pattern as the factory's 08041F34/08042850: blank (0) is no character, any pattern it
    // does not know is '-', never an error of its own.
    static char character(uint8_t segment);
    // What the five digit positions hold, the sign (bit 4 of the first) apart: digits and blanks with
    // at least one digit (Number); only blanks and dashes (Blank: the DMM's "--.", dashes or an empty
    // display); anything else, a dash or an unknown pattern among digits or a letter (Other).
    enum class Digits : uint8_t { Number, Blank, Other };
    // The same from a decoded main text (a factory record): a leading '-' before a digit or before
    // ".digit" is the sign.
    static Digits digits(const char *text);
    // The main text for a record or a serial line: the text, except where the reading is not a value
    // but its text alone would read back as one (a dash or unknown top segment without the sign bit:
    // "-1234"); then "?", which reads back Unsupported. The Result itself is not changed.
    static const char *recordText(const Result &result);
    // The primary reading, one rule for packets and factory records: any 'L' Overload (the factory's
    // strstr "L"); Blank NoReading (no number, not an error); Number in a known unit Ok; Other, or a
    // number without a known unit, Unsupported (letters, mixed or unknown segments: never a value).
    // The quantity comes from the unit in every case.
    static void classify(const char *text, const char *rawUnit, Digits digits, NumericValue &primary);
    // The range mode: AUTO (payload[10].2) first, then MANU (.3). Without either, a packet whose
    // primary (after classify) is a value, OL or NoReading, not Unsupported, of a ranged function
    // whose unit is of that function's quantity (Functions::quantity: any known prefix, uA and F
    // included) is in a manual range: manual ranges can come with neither bit (V/A/Ohm on our ZT703S;
    // capacitance on the ZT703S of issue #11: mode Auto 1 -> Unknown -1 after RANGE, index 1). The
    // mode is metadata apart from the number: NoReading stays no value. Which range it is stays the
    // tables' (Ranges::current), never guessed.
    // Diode, continuity, an unknown function, a unit of another or no quantity and an unreadable
    // (Unsupported) packet: Unknown.
    static RangeMode range(bool autoBit, bool manualBit, const Result &result);

private:
    static Digits positions(const char *characters); // Digit positions only: a '-' is a segment.
    static bool number(const uint8_t *payload, unsigned first, unsigned count, char *text, size_t capacity);
};
} // namespace openosc::dmm
