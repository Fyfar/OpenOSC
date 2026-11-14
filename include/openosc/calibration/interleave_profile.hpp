#pragma once
#include "openosc/calibration/data.hpp"
#include "openosc/acquisition/frame.hpp"

namespace openosc::calibration {
// The ZT706S service interleave offset: the codes added to the HIGH ADC's points of an interleaved
// CH1 record, in place of the factory record's float (CalibrationData::interleaveCorrection) while
// enabled. Set in StepCodes; a value beyond LimitCodes is no offset between two working ADCs. Only
// a record's own metadata may carry it (never a live value read at display time); a reader applies
// it as one whole code (scope::InterleaveCorrection::shift), so tenths only choose the rounding.
struct InterleaveProfile {
    static constexpr float LimitCodes = scope::InterleaveCorrection::Limit, StepCodes = .1f;
    uint16_t factoryCrc = 0;
    bool enabled = false;
    float offset = 0;
    static bool allowed(float codes) { return scope::InterleaveCorrection::valid(codes); } // NaN: no.
    bool valid(const CalibrationData &factory) const;
};
} // namespace openosc::calibration
