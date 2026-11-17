#pragma once
#include "openosc/board/current.hpp"
#include "openosc/measurement/dmm/function.hpp"

namespace openosc::dmm {
struct RangeInfo {
    const char *unit;
    unsigned decimals;
    double maximumSi;
};
class Ranges {
public:
    // The board's ranges of a function (board::Current), in RANGE order; the other model's for tests.
    static const RangeInfo *list(Function function, unsigned &count);
    static const RangeInfo *list(Function function, board::DmmCounts counts, unsigned &count);
    static const RangeInfo *current(const Result &result, unsigned *index = nullptr);
    static bool choose(Function function, double maximumSi, unsigned &index);
    // The AUTO/RANGE keys apply: the function has a range table (Functions::ranged) the keys step through.
    // Diode, continuity and an unknown function (no reading yet) have none; capacitance has a table (for
    // the scale and the reading's range) but the chip ranges it by itself and the factory firmware
    // ignores the key there (ZT703S, stock 1.6.27). Every range command is refused in these.
    static bool selectable(Function function) { return Functions::ranged(function) && function != Function::Capacitance; }
};
// A manual range reached with RANGE (20 20) presses, one at a time: each press waits until a fresh
// packet shows its effect. The first press from Auto waits up to ManualMs for Manual (Decoder::range;
// the controller can still send Auto packets for a while); a fresh packet still in AUTO after that:
// Unsupported after that single press, judged on a packet first seen ManualMs after it (an early
// packet then silence, or packets without a range state, leave it to DeadlineMs). A transitional
// unknown function is waited out; another known function ends it (InvalidArgument). Once
// Manual, each further press waits up to StepMs for the next range index. At most one press per range
// of the function, and the whole operation within DeadlineMs (the bound SCPI waits for: below
// scpi::Engine's 7 s operation wait).
class RangeSelector {
public:
    static constexpr uint32_t SettleMs = 400, ManualMs = 2500, StepMs = 1500, DeadlineMs = 6000;
    bool request(unsigned index, const Result &latest, uint32_t now);
    bool poll(const Result &latest, uint32_t now, CommandGroup &command);
    void sent(uint32_t sequence, uint32_t now) { sequence_ = seen_ = sequence; sentAt_ = seenAt_ = now; waiting_ = true; ++attempts_; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
    unsigned attempts() const { return attempts_; }
    void cancel() { status_ = DataStatus::NoData; }
    void fail() { status_ = DataStatus::HardwareError; }
private:
    Function function_ = Function::Unknown;
    unsigned target_ = 0, attempts_ = 0, maximum_ = 0, index_ = 0;
    uint32_t sequence_ = 0, sentAt_ = 0, deadline_ = 0;
    uint32_t seen_ = 0, seenAt_ = 0; // The latest sequence observed and when (live packets vs silence).
    bool waiting_ = false, manual_ = false; // manual_: the controller has shown MANU since the request.
    DataStatus status_ = DataStatus::NoData;
};
} // namespace openosc::dmm
