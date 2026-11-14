#pragma once
#include "openosc/acquisition/frame.hpp"
#include <algorithm>
#include <cmath>

namespace openosc::scope {
// A held record keeps the trigger it was shown with, whatever is staged for the next capture.
class HeldTrigger {
public:
    void forget() { known_ = false; }
    void remember(uint32_t sequence, bool found, uint32_t index) {
        sequence_ = sequence; found_ = found; index_ = index; known_ = true;
    }
    bool recall(uint32_t sequence, bool &found, uint32_t &index) const {
        if (!known_ || sequence_ != sequence) return false;
        found = found_; index = index_;
        return true;
    }

private:
    uint32_t sequence_ = 0, index_ = 0;
    bool known_ = false, found_ = false;
};
class TriggerLevel {
public:
    // The level, set in the codes of the next capture, as a code of a record captured under other settings.
    static double inFrame(double level, const ChannelMetadata &settings, const ChannelMetadata &frame) {
        if (!settings.calibrated || !frame.calibrated || settings.voltsPerCode <= 0 || frame.voltsPerCode <= 0)
            return level;
        return (level - settings.zeroCode) * settings.voltsPerCode / frame.voltsPerCode + frame.zeroCode;
    }
};
// Moving a channel's analog zero shifts all its codes, so a trigger on that channel has to shift
// with them to stay at the same voltage. Levels are whole codes: the remainder is carried over so
// that many small moves do not add up to a drift. Nothing changes until the move has been applied,
// and the remainder only belongs to the level, channel and zero it was computed for.
class TriggerFollow {
public:
    struct Proposal {
        int16_t level = 0, position = 0;
        uint8_t channel = 0;
        double remainder = 0;
    };
    Proposal propose(int16_t level, unsigned channel, int position, int nextPosition, double zeroBefore, double zeroAfter) const {
        const double carried = owned_ && channel_ == channel && level_ == level && position_ == position ? remainder_ : 0;
        const double exact = level + carried + zeroAfter - zeroBefore;
        const double limited = std::clamp(exact, -128.0, 127.0);
        const auto next = int16_t(std::lround(limited));
        return {next, int16_t(nextPosition), uint8_t(channel), exact == limited ? exact - next : 0};
    }
    void commit(const Proposal &proposal) {
        level_ = proposal.level; position_ = proposal.position; channel_ = proposal.channel;
        remainder_ = proposal.remainder; owned_ = true;
    }
    // Any other change of the trigger or of its channel's zero starts over.
    void observe(int16_t level, unsigned channel, int position) {
        if (owned_ && (level_ != level || channel_ != channel || position_ != position)) owned_ = false;
    }

private:
    double remainder_ = 0;
    int16_t level_ = 0, position_ = 0;
    uint8_t channel_ = 0;
    bool owned_ = false;
};
} // namespace openosc::scope
