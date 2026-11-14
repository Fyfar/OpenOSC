#pragma once
#include "openosc/measurement/dmm/result.hpp"
#include "openosc/measurement/dmm/relative.hpp"
#include <cstddef>
#include <cstring>

namespace openosc::dmm {
enum class Function : unsigned; // function.hpp
// Running statistics of the factory firmware: min, max and sum/count over every valid reading
// since the last reset, no time window and no cap (the trend's 260 slots are separate). Values
// are SI, so a range change alone keeps them. docs/DMM_STATISTICS.md has the reset matrix.
class Statistics {
public:
    bool push(const NumericValue &value);
    void reset() {
        count_ = 0;
        min_ = max_ = sum_ = correction_ = 0;
        quantity_ = Quantity::Unknown;
    }
    uint64_t count() const { return count_; }
    double minimum() const { return min_; }
    double maximum() const { return max_; }
    double average() const { return count_ ? sum_ / count_ : 0; }
    Quantity quantity() const { return quantity_; }

private:
    uint64_t count_ = 0;
    double min_ = 0, max_ = 0, sum_ = 0, correction_ = 0;
    Quantity quantity_ = Quantity::Unknown;
};
// MIN/MAX/AVG as the DMM screen shows them: the period's values when they are of the shown
// reading's quantity (else none), less the local REL offset (RelativeReading::offset; a controller
// REL is already in the values). One view for the screen and SCPI, no second accumulator.
struct StatisticsView {
    bool valid = false;
    uint64_t count = 0;
    double minimum = 0, maximum = 0, average = 0;
    Quantity quantity = Quantity::Unknown;
    static StatisticsView of(const Statistics &statistics, Quantity shown, double offset);
};
class History {
public:
    static constexpr size_t Capacity = 260;
    void push(double value);
    bool value(size_t oldestIndex, double &output) const;
    size_t size() const { return size_; }
    void reset() { cursor_ = size_ = 0; }

private:
    double samples_[Capacity]{};
    size_t cursor_ = 0, size_ = 0;
};
class Session {
public:
    bool accept(const uint8_t *packet, size_t bytes, uint64_t timestampUs);
    bool hold(bool enabled);
    bool held() const { return userHold_ || latest_.packetHold; }
    bool userHeld() const { return userHold_; }
    DataStatus status(uint64_t nowUs, uint64_t staleAfterUs) const;
    const Result &latest() const { return latest_; }
    const Result &displayed() const { return frozen_ ? held_ : latest_; }
    const Statistics &statistics() const { return stats_; }
    const History &history() const { return history_; }
    const RelativeReading &relative() const { return relative_; }
    // An accepted REL command restarts the statistics, as the factory's, even when REL was already so.
    bool relative(bool enabled) {
        if (enabled != relative_.active()) {
            if (enabled) { if (!relative_.capture(displayed())) return false; }
            else relative_.clear();
        }
        resetStatistics(); return true;
    }
    void resetStatistics() {
        stats_.reset();
        history_.reset();
    }
    // An accepted function selection: the statistics restart, and only readings of `function`
    // (or any, once the packet time passes `untilUs`, when the selection did not take) seed them;
    // packets of the old function still in flight do not.
    void restart(Function function, uint64_t untilUs);
    bool awaitingFunction() const { return awaiting_; }
    // The factory's no-reading placeholder: "--." in the first three characters.
    static bool placeholder(const Result &result) { return !std::strncmp(result.mainText, "--.", 3); }

private:
    Result latest_, held_;
    Statistics stats_;
    History history_;
    RelativeReading relative_;
    // The context of the accumulated period: the last reading taken outside HOLD.
    unsigned function_ = ~0U;        // Its decoded Function (~0: none yet).
    bool functionCurrent_ = false, contextPeerRelative_ = false, contextKnown_ = false;
    Function awaited_{};
    uint64_t awaitUntilUs_ = 0;
    Quantity trendQuantity_ = Quantity::Unknown; // The trend restarts on a quantity of its own.
    uint32_t sequence_ = 0;
    bool userHold_ = false, frozen_ = false, awaiting_ = false;
};
enum class CommandGroup : uint8_t { Voltage, ResistanceGroup, Current, Millivolts, Milliamps,
                                  PeerHold, AutoRange, NextRange, PeerRelative };
class Command {
public:
    static bool encode(CommandGroup group, uint8_t (&bytes)[2]);
};
} // namespace openosc::dmm
