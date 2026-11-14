#pragma once
#include <cstdint>

namespace openosc {
// When the independent watchdog may be fed (docs/WATCHDOG.md). Pure: times in ms of one clock that
// wraps (unsigned differences), no registers.
//
// - A heartbeat is the main loop's proof of progress: one per pass of the application's or
//   Recovery's loop (and the start, which stands for the boot until the first pass).
// - progress() is a long operation's proof inside one pass: a completed NOR sector, a progress
//   report of an update, format or FAT check. It counts only for `limit` from the last heartbeat
//   (OperationMs, or what expect() declared), so I/O repeated forever inside one pass still ends.
// - feeds(): servicing in waits (Board::service) feeds only for StallMs after the last progress.
//   A loop that keeps servicing without progress therefore stops feeding, and the watchdog resets
//   one timeout later; a wait that never services stops feeding at once.
class WatchdogPolicy {
public:
    // Above the longest bounded wait between two progress points (a NOR status poll's 5 s timeout
    // ends within StallMs plus the watchdog's own timeout) and far above a normal loop pass.
    static constexpr uint32_t StallMs = 3000;
    // A pass's whole budget for progress points without a new heartbeat (boot, a file save, a
    // screenshot, a FAT format step), unless the operation declared more.
    static constexpr uint32_t OperationMs = 30000;
    // The most any operation may declare (an update, a FAT check of the whole NOR).
    static constexpr uint32_t MaximumOperationMs = 600000;

    void begin(uint32_t now) { halted_ = false; heartbeat(now); }
    void heartbeat(uint32_t now) {
        if (halted_) return;
        anchor_ = progress_ = now; limit_ = OperationMs;
    }
    void progress(uint32_t now) {
        if (!halted_ && uint32_t(now - anchor_) < limit_) progress_ = now;
    }
    // An operation that may take up to `ms` from the last heartbeat: raises the pass's limit (never
    // past MaximumOperationMs from that heartbeat, however often it is declared) and counts as progress.
    void expect(uint32_t now, uint32_t ms) {
        if (halted_) return;
        const uint32_t limit = ms < MaximumOperationMs ? ms : MaximumOperationMs;
        if (limit > limit_) limit_ = limit;
        progress(now);
    }
    bool feeds(uint32_t now) const { return !halted_ && uint32_t(now - progress_) < StallMs; }
    uint32_t sinceProgress(uint32_t now) const { return now - progress_; }
    void halt() { halted_ = true; }
    bool halted() const { return halted_; }

private:
    uint32_t anchor_ = 0, progress_ = 0, limit_ = OperationMs;
    bool halted_ = false;
};
} // namespace openosc
