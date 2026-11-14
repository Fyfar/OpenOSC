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
};
// A manual range reached with RANGE (20 20) presses, one at a time: each press waits until a fresh
// packet shows its effect. The first press from Auto waits up to ManualMs for MANU (the controller can
// still send Auto packets for a while); no MANU by then: Unsupported after that single press. Once
// Manual, each further press waits up to StepMs for the next range index. At most one press per range
// of the function, and the whole operation within DeadlineMs (the bound SCPI waits for: below
// scpi::Engine's 7 s operation wait).
class RangeSelector {
public:
    static constexpr uint32_t SettleMs = 400, ManualMs = 2500, StepMs = 1500, DeadlineMs = 6000;
    bool request(unsigned index, const Result &latest, uint32_t now);
    bool poll(const Result &latest, uint32_t now, CommandGroup &command);
    void sent(uint32_t sequence, uint32_t now) { sequence_ = sequence; sentAt_ = now; waiting_ = true; ++attempts_; }
    bool busy() const { return status_ == DataStatus::Busy; }
    DataStatus status() const { return status_; }
    unsigned attempts() const { return attempts_; }
    void cancel() { status_ = DataStatus::NoData; }
    void fail() { status_ = DataStatus::HardwareError; }
private:
    Function function_ = Function::Unknown;
    unsigned target_ = 0, attempts_ = 0, maximum_ = 0, index_ = 0;
    uint32_t sequence_ = 0, sentAt_ = 0, deadline_ = 0;
    bool waiting_ = false, manual_ = false; // manual_: the controller has shown MANU since the request.
    DataStatus status_ = DataStatus::NoData;
};
} // namespace openosc::dmm
