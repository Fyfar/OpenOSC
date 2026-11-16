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
    // A group key steps the controller within its group (DC/AC of V, mV, A and mA; Ohm, continuity,
    // diode, capacitance in turn), and it can still report the old function long after the key: a key
    // pressed again on time alone lands on the other DC/AC (the factory's restore 0804BE00/0804D2C4
    // presses on every mismatch, without a deadline: "AC/DC amps forever"). So each further key waits
    // until the last one has shown: a packet after it whose recognised function is known and differs
    // from the one the key was pressed on (an old packet, Unknown, or mA/uA within one function is
    // not). A key that never shows ends in one Timeout, not a series of blind toggles. KeySettleMs is
    // still the least spacing between keys.
    static constexpr uint32_t KeySettleMs = 900;
    static constexpr uint32_t DeadlineMs = 6500; // A selection the controller has not shown by then has failed.
    // The key poll() returned went out: its origin is the function poll() saw (a UART still busy
    // sends nothing, and poll() prepares again).
    void sent(uint32_t sequence, uint32_t now) { sequence_ = sequence; nextAt_ = now + KeySettleMs; origin_ = prepared_; ++attempts_; }
    void cancel() { status_ = DataStatus::NoData; }
    void fail() { status_ = DataStatus::HardwareError; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
private:
    Function requested_ = Function::Unknown;
    DataStatus status_ = DataStatus::NoData;
    uint32_t deadline_ = 0, nextAt_ = 0, sequence_ = 0;
    Function origin_ = Function::Unknown, prepared_ = Function::Unknown; // Shown when the last key went / goes out.
    unsigned attempts_ = 0;
    uint64_t minimumTimestampUs_ = 0;
};
} // namespace openosc::dmm
