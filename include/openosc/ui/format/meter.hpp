#pragma once
#include "openosc/measurement/dmm/result.hpp"
#include <cstring>

namespace openosc::ui {
struct MeterText {
    static const char *unit(const char *unit) {
        if (!std::strcmp(unit, "Ohm")) return "Ω";
        if (!std::strcmp(unit, "kOhm")) return "kΩ";
        if (!std::strcmp(unit, "MOhm")) return "MΩ";
        if (!std::strcmp(unit, "uA")) return "µA";
        if (!std::strcmp(unit, "uF")) return "µF";
        return std::strlen(unit) <= 4 ? unit : "?";
    }
    static const char *function(const dmm::Result &result) {
        const bool current = result.primary.quantity == Quantity::Current;
        switch (result.kind) {
        case dmm::Kind::Dc: return current ? "DC current" : "DC voltage";
        case dmm::Kind::Ac: return current ? "AC current" : "AC voltage";
        case dmm::Kind::Resistance: return "Resistance";
        case dmm::Kind::Capacitance: return "Capacitance";
        case dmm::Kind::Diode: return "Diode";
        case dmm::Kind::DiodeWithBeeper: return "Diode + Beep"; // Both factory annunciators (docs/DMM_DIODE_BEEPER.md).
        case dmm::Kind::Continuity: return "Continuity";
        default: return "Measurement";
        }
    }
};
} // namespace openosc::ui
