#include "openosc/measurement/dmm/function.hpp"
#include <cstring>

namespace openosc::dmm {
Function Functions::identify(const Result &r) {
    switch (r.kind) {
    case Kind::Resistance: return Function::Resistance;
    case Kind::Capacitance: return Function::Capacitance;
    case Kind::Diode:
    case Kind::DiodeWithBeeper: return Function::Diode; // The diode with the beeper flag: the same function.
    case Kind::Continuity: return Function::Continuity;
    case Kind::Dc:
    case Kind::Ac: {
        const bool ac = r.kind == Kind::Ac;
        if (r.primary.quantity == Quantity::Current) {
            const bool fine = !std::strcmp(r.unit, "mA") || !std::strcmp(r.unit, "uA");
            return fine ? (ac ? Function::MilliampsAc : Function::MilliampsDc) : (ac ? Function::CurrentAc : Function::CurrentDc);
        }
        if (r.primary.quantity == Quantity::Voltage) {
            const bool fine = !std::strcmp(r.unit, "mV") || !std::strcmp(r.unit, "uV");
            return fine ? (ac ? Function::MillivoltsAc : Function::MillivoltsDc) : (ac ? Function::VoltageAc : Function::VoltageDc);
        }
        return Function::Unknown;
    }
    default: return Function::Unknown;
    }
}
CommandGroup Functions::group(Function f) {
    if (f == Function::VoltageDc || f == Function::VoltageAc) return CommandGroup::Voltage;
    if (f >= Function::Resistance && f <= Function::Capacitance) return CommandGroup::ResistanceGroup;
    if (f == Function::CurrentDc || f == Function::CurrentAc) return CommandGroup::Current;
    if (f == Function::MillivoltsDc || f == Function::MillivoltsAc) return CommandGroup::Millivolts;
    return CommandGroup::Milliamps;
}
Function Functions::next(CommandGroup group, Function current) {
    static constexpr Function resistance[] = {Function::Resistance, Function::Continuity, Function::Diode, Function::Capacitance};
    if (group == CommandGroup::ResistanceGroup) {
        for (unsigned i = 0; i < 4; ++i) if (resistance[i] == current) return resistance[(i + 1) % 4];
        return resistance[0];
    }
    const auto first = group == CommandGroup::Voltage ? Function::VoltageDc : group == CommandGroup::Current ? Function::CurrentDc :
                       group == CommandGroup::Millivolts ? Function::MillivoltsDc : Function::MilliampsDc;
    return current == first ? Function(unsigned(first) + 1) : first;
}
bool FunctionSelector::request(Function function, const Result &latest, uint32_t now, uint64_t minimumTimestampUs) {
    if (function >= Function::Unknown || (busy() && function != requested_)) return false;
    if (busy()) return true;
    requested_ = function; attempts_ = 0; sequence_ = latest.sequence; nextAt_ = now; deadline_ = now + DeadlineMs;
    minimumTimestampUs_ = minimumTimestampUs;
    status_ = latest.sequence && latest.timestampUs >= minimumTimestampUs_ && Functions::identify(latest) == function ? DataStatus::Ok : DataStatus::Busy;
    return true;
}
bool FunctionSelector::poll(const Result &latest, uint32_t now, CommandGroup &command) {
    if (!busy()) return false;
    if (int32_t(now - deadline_) >= 0) { status_ = DataStatus::Timeout; return false; }
    if (latest.timestampUs < minimumTimestampUs_) return false;
    if (latest.sequence && Functions::identify(latest) == requested_) { status_ = DataStatus::Ok; return false; }
    if (!latest.sequence || int32_t(now - nextAt_) < 0 || (attempts_ && latest.sequence == sequence_)) return false;
    if (attempts_ >= 6) { status_ = DataStatus::Timeout; return false; }
    command = Functions::group(requested_); return true;
}
} // namespace openosc::dmm
