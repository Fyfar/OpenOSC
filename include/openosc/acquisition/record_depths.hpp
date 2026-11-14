#pragma once
#include "openosc/acquisition/frame.hpp"
#include <cstdint>

namespace openosc::scope {
// Record memory is a budget of 8-bit samples shared by the enabled channels: two channels
// split it, one channel gets all of it (one byte per point). The menu and settings choose
// this total; everything that describes a record (metadata, SCPI, files) counts per channel.
struct RecordBudget {
    static constexpr uint32_t Steps[] = {65536, 131072, 262144, 524288};
    static constexpr unsigned StepCount = sizeof Steps / sizeof Steps[0];
    static constexpr uint32_t Minimum = Steps[0], Maximum = Steps[StepCount - 1], Default = Steps[0];
    static constexpr bool valid(uint32_t points) {
        for (auto step : Steps) if (step == points) return true;
        return false;
    }
    static constexpr unsigned channels(bool first, bool second) { return unsigned(first) + unsigned(second); }
    // Points of each enabled channel; zero when no channel is enabled or the budget is not listed.
    static constexpr uint32_t perChannel(uint32_t points, unsigned channels) {
        return valid(points) && (channels == 1 || channels == 2) ? points / channels : 0;
    }
    // The smallest budget that gives every enabled channel at least `points`; zero when none does.
    // Legacy settings and SCPI ACQ:MDEP name a depth per channel; this keeps at least that depth.
    static constexpr uint32_t covering(uint32_t points, unsigned channels) {
        for (auto step : Steps) if (perChannel(step, channels) >= points) return step;
        return 0;
    }
    // The listed budget moved by a signed number of steps, clamped at both ends.
    static constexpr uint32_t step(uint32_t points, int steps) {
        int index = 0;
        while (index < int(StepCount) - 1 && Steps[index] < points) ++index;
        index += steps;
        return Steps[index < 0 ? 0 : index >= int(StepCount) ? StepCount - 1 : index];
    }
    // Settings has one key per value, so this one wraps from the largest budget to the smallest.
    static constexpr uint32_t next(uint32_t points) {
        const uint32_t following = step(points, 1);
        return following == points ? Steps[0] : following;
    }
};
// Depth of one channel's record: what the capture, sample plan and SCPI windows count.
struct RecordDepth {
    static constexpr uint32_t Minimum = 4, Granule = 4; // One point per DMA lane.
    static constexpr uint32_t Maximum = RecordBudget::Maximum; // One channel, the full budget.
    static constexpr bool valid(uint32_t depth) { return depth >= Minimum && depth <= Maximum && !(depth % Granule); }
};
constexpr bool recordBudgetsValid() {
    for (unsigned i = 0; i < RecordBudget::StepCount; ++i) {
        const auto step = RecordBudget::Steps[i];
        if ((i && RecordBudget::Steps[i - 1] >= step) || !RecordDepth::valid(step / 2) || !RecordDepth::valid(step)) return false;
    }
    return true;
}
static_assert(recordBudgetsValid(), "Budgets ascend and split into valid per-channel depths");
static_assert(RecordBudget::valid(RecordBudget::Default), "The default budget is a menu step");
static_assert(RecordDepth::Maximum <= FrameMetadata::MaximumRing, "Frames cannot address the deepest record");
} // namespace openosc::scope
