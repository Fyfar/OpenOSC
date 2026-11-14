#pragma once
#include "openosc/measurement/dmm/session.hpp"

namespace openosc::dmm {
enum class Function : unsigned { VoltageDc, VoltageAc, Resistance, Continuity, Diode, Capacitance,
    CurrentDc, CurrentAc, MillivoltsDc, MillivoltsAc, MilliampsDc, MilliampsAc, Unknown };
class Functions {
public:
    static Function identify(const Result &result);
    static CommandGroup group(Function function);
    static Function next(CommandGroup group, Function current);
};
class FunctionSelector {
public:
    bool request(Function function, const Result &latest, uint32_t now, uint64_t minimumTimestampUs = 0);
    bool poll(const Result &latest, uint32_t now, CommandGroup &command);
    // The controller can still report the old function 400 ms after a key; pressing again
    // then overshoots the requested function and switches its relays needlessly.
    static constexpr uint32_t KeySettleMs = 900;
    static constexpr uint32_t DeadlineMs = 6500; // A selection the controller has not shown by then has failed.
    void sent(uint32_t sequence, uint32_t now) { sequence_ = sequence; nextAt_ = now + KeySettleMs; ++attempts_; }
    void cancel() { status_ = DataStatus::NoData; }
    void fail() { status_ = DataStatus::HardwareError; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
private:
    Function requested_ = Function::Unknown;
    DataStatus status_ = DataStatus::NoData;
    uint32_t deadline_ = 0, nextAt_ = 0, sequence_ = 0;
    unsigned attempts_ = 0;
    uint64_t minimumTimestampUs_ = 0;
};
} // namespace openosc::dmm
