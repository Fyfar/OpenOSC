#pragma once
#include "openosc/measurement/dmm/result.hpp"

namespace openosc::dmm {
class RelativeReading {
public:
    bool capture(const Result &result) {
        if (!result.primary.valid()) return false;
        reference_ = result.primary; kind_ = functionKind(result.kind); active_ = true; return true;
    }
    void clear() { active_ = false; reference_ = {}; kind_ = Kind::Unknown; }
    bool active() const { return active_; }
    const NumericValue &reference() const { return reference_; }
    // Per physical function: the diode keeps its reference when the beeper flag comes and goes.
    bool matches(const Result &result) const { return active_ && functionKind(result.kind) == kind_ && result.primary.quantity == reference_.quantity; }
    // What a local REL subtracts from this reading and its statistics (0 when it does not apply).
    double offset(const Result &result) const { return matches(result) ? reference_.si : 0; }
    NumericValue apply(const Result &result) const {
        auto value = result.primary;
        if (matches(result) && value.valid()) value.si -= reference_.si;
        return value;
    }
private:
    NumericValue reference_;
    Kind kind_ = Kind::Unknown;
    bool active_ = false;
};
} // namespace openosc::dmm
