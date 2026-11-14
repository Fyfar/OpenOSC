#pragma once
#include "openosc/acquisition/arena.hpp"
#include "openosc/acquisition/circular_progress.hpp"
#include <algorithm>
#include <cstdint>

namespace openosc::scope {
// The copy schedule of a split capture ring (CaptureBuffer::split), counted in transfers of one lane.
//
// Every lane rings through laneTransfers words of AHB SRAM in one circular DMA segment, as the
// all-AHB layout that kept the full rate under USB traffic (HIL 2026-11-10). Trigger n (n = 1, 2, ...)
// comes for every lane when lane 0 has made n * copyTransfers requests (HistoryCopy paces it from lane
// 0's DMAMUX events; lane 3 copies on lane 1's trigger). A trigger has the lane's MDMA channel copy
// ring block n + Lead to the lane's history while the lanes write block n: they reach block n + Lead
// Lead blocks later, which is the copy's deadline.
//
// A trigger copies the block's previous pass: ring cursor c holds chronological lane word
// c - laneTransfers, which goes to history index (c - laneTransfers) mod historyTransfers. A frozen
// lane that wrote E words and had its ring copied up to cursor C (the next block's start) has words
// [max(0, C - laneTransfers - historyTransfers), E): the newest laneTransfers in the ring, the older
// ones in the history. The blocks copied ahead are in both, so the record is at least
// laneTransfers + historyTransfers - (Lead + 1) * copyTransfers - 2 words per lane.
//
// The first pass of the ring holds only what was in RAM before the capture (cursor below
// laneTransfers: chronological words below 0, never in a record), so its copies only ramp the MDMA's
// load up in RampNodes steps of a part of each block, skipping the rest of the block through the
// block-repeat address update; the history stays block-aligned. From the first wrap on, when the
// first real words are due, every block is copied whole.
struct HistorySchedule {
    static constexpr uint32_t Lead = 1;
    static constexpr uint32_t EventRequests = 32; // DMAMUX NBREQ + 1, at its largest.
    // Cursor up to which every lane's ring has been copied after `triggers` triggers.
    static uint64_t copied(const CaptureBuffer &b, uint64_t triggers) { return (triggers + Lead + 1) * b.copyTransfers; }
    // The oldest chronological lane word a frozen record holds, copied up to `copied`.
    static uint64_t oldest(const CaptureBuffer &b, uint64_t copied) {
        const uint64_t span = uint64_t(b.laneTransfers) + b.historyTransfers;
        return copied > span ? copied - span : 0;
    }
    static uint32_t capacity(const CaptureBuffer &b) {
        return 4 * (b.laneTransfers + b.historyTransfers - (Lead + 1) * b.copyTransfers - 2);
    }
    // History blocks; the MDMA writes them in order, so the next one tells the triggers served.
    static uint32_t cycle(const CaptureBuffer &b) { return b.historyTransfers / b.copyTransfers; }
    static constexpr unsigned RampNodes = 3;
    // One lane's MDMA program: RampNodes nodes over the first pass from block Lead + 1, each moving
    // `bytes` of a block, then historyTransfers / laneTransfers nodes, each one whole pass of the ring
    // into the next part of the history, linked in a circle. Every trigger copies one block (TRGM
    // block, BRC = blocks - 1); a node's source and destination advance by whole blocks.
    struct Node { uint32_t ringBlock = 0, historyBlock = 0, blocks = 0, bytes = 0, next = 0; };
    static unsigned passes(const CaptureBuffer &b) { return b.historyTransfers / b.laneTransfers; }
    static unsigned nodes(const CaptureBuffer &b) { return RampNodes + passes(b); }
    static Node node(const CaptureBuffer &b, unsigned index) {
        const uint32_t ring = b.laneTransfers / b.copyTransfers, block = b.copyTransfers * 2;
        if (index < RampNodes) {
            const uint32_t firstPass = ring - Lead - 1, share = firstPass / RampNodes;
            const uint64_t cursor = uint64_t(Lead + 1) * b.copyTransfers + b.historyTransfers - b.laneTransfers;
            const uint32_t history = uint32_t(cursor % b.historyTransfers / b.copyTransfers);
            const uint32_t bytes = std::max<uint32_t>(64, block * (index + 1) / (RampNodes + 1) / 64 * 64);
            return {Lead + 1 + index * share, history + index * share, index + 1 < RampNodes ? share : firstPass - index * share,
                    bytes, index + 1};
        }
        const uint32_t pass = index - RampNodes;
        return {0, pass * ring, ring, block, RampNodes + (pass + 1) % passes(b)};
    }
    static Node first(const CaptureBuffer &b) { return node(b, 0); }
    // Triggers served modulo cycle(), from the history block the MDMA writes next.
    static uint32_t served(const CaptureBuffer &b, uint32_t nextHistoryBlock) {
        return (nextHistoryBlock + cycle(b) - first(b).historyBlock) % cycle(b);
    }
    // The same from the byte offset of a lane's MDMA destination (CDAR) in its history: while the
    // next node loads, the history's end stands for its start.
    static bool servedAt(const CaptureBuffer &b, uint32_t offset, uint32_t &out) {
        const uint32_t block = b.copyTransfers * 2;
        if (offset % block || offset / block > cycle(b)) return false;
        out = served(b, offset / block % cycle(b));
        return true;
    }
    // Triggers every lane's channel had when lane 0's DMAMUX counted `counted` requests: one per block.
    static uint64_t triggers(const CaptureBuffer &b, uint64_t counted) { return counted / b.copyTransfers; }
    // Whether a lane may have served `served` triggers (modulo the cycle) at that count: exactly the
    // block triggers.
    static bool possible(const CaptureBuffer &b, uint64_t counted, uint32_t served, uint64_t &made) {
        const uint64_t n = triggers(b, counted);
        if (n % cycle(b) != served) return false;
        made = n;
        return true;
    }
    // The frozen record of lanes that wrote `lanes` words once each lane's MDMA channel served
    // `served[lane]` triggers (modulo the cycle). DMAMUX counted the lane-0 requests the DMA transferred,
    // or one more that reached it as the timer stopped, and each lane's triggers follow from that count.
    // Each lane's ring must have been copied ahead of it by at most the schedule's lead. False when
    // any lane's copies are out of step.
    static bool window(const CaptureBuffer &b, const uint64_t (&lanes)[4], const uint32_t (&served)[4],
                       CircularProgress::Window &out, SplitHistory &split) {
        const uint32_t block = b.copyTransfers;
        uint64_t first = 0;
        for (unsigned lane = 0; lane < 4; ++lane) {
            uint64_t made = UINT64_MAX, n = 0;
            for (uint64_t counted = lanes[0]; counted <= lanes[0] + 1; ++counted)
                if (possible(b, counted, served[lane], n)) made = n;
            if (made == UINT64_MAX) return false;
            const uint64_t cursor = copied(b, made), written = lanes[lane];
            if (!written || cursor <= written || cursor - written > uint64_t(Lead + 1) * block + 2) return false;
            // Lane `lane` holds points 4t+lane from t = oldest on: every point from 4*oldest+lane-3 on
            // whose lane is this one does.
            const uint64_t from = oldest(b, cursor) * 4 + lane;
            first = std::max<uint64_t>(first, from >= 3 ? from - 3 : 0);
        }
        CircularProgress::Window window;
        window.first = first;
        window.end = CircularProgress::prefix(lanes);
        if (window.end <= window.first) return false;
        const uint32_t laneRing = b.laneTransfers + b.historyTransfers;
        SplitHistory result;
        result.ahbWords = b.laneTransfers;
        result.historyWords = b.historyTransfers;
        for (unsigned lane = 0; lane < 4; ++lane) {
            result.older[lane] = b.history[lane];
            result.end[lane] = uint32_t(lanes[lane] % laneRing);
            result.newest[lane] = uint32_t((lanes[lane] - 1) % b.historyTransfers);
        }
        out = window;
        split = result;
        return true;
    }
    // The schedule's own limits: the lane ring a power of two in one DMA segment, whole blocks of
    // whole pacing transfers, the first node not empty, the MDMA block repeat count (12 bits).
    static bool valid(const CaptureBuffer &b) {
        const uint32_t ring = b.copyTransfers ? b.laneTransfers / b.copyTransfers : 0;
        return b.words() && b.laneTransfers && !(b.laneTransfers & (b.laneTransfers - 1)) &&
               b.segmentTransfers == b.laneTransfers && b.copyTransfers && !(b.copyTransfers % EventRequests) &&
               !(b.laneTransfers % b.copyTransfers) && ring >= Lead + 1 + RampNodes && ring <= 4096 &&
               b.copyTransfers * 2 >= (RampNodes + 1) * 64 && // Ramp parts of whole 64-byte MDMA buffers.
               b.historyTransfers >= b.laneTransfers && !(b.historyTransfers % b.laneTransfers) &&
               b.copyTransfers * 2 <= 65536;
    }
};
} // namespace openosc::scope
