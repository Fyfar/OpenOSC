#pragma once
#include "openosc/acquisition/arena.hpp"
#include <algorithm>
#include <cstdint>

namespace openosc::scope {
class CircularProgress {
public:
    struct Window { uint64_t first=0,end=0; uint64_t size() const { return end-first; } };
    // Lane c owns chronological indices 4*n+c. End is the first missing word.
    static uint64_t prefix(const uint64_t (&lanes)[4]) {
        uint64_t end=UINT64_MAX;
        for (unsigned lane=0;lane<4;++lane) {
            if (lanes[lane]>(UINT64_MAX-3)/4) return 0;
            end=std::min(end,lanes[lane]*4+lane);
        }
        return end;
    }
    // After stopping all DMA streams, exclude every overwritten word, including
    // a partial oldest group when the four streams did not stop in the same cycle.
    static Window window(const uint64_t (&lanes)[4],uint32_t wordsPerBank) {
        if (!wordsPerBank || wordsPerBank>AcquisitionArena::MaximumLaneTransfers) return {};
        Window result; result.end=prefix(lanes);
        if (!result.end) return {};
        for (unsigned lane=0;lane<4;++lane) if (lanes[lane]>wordsPerBank)
            result.first=std::max(result.first,(lanes[lane]-wordsPerBank-1)*4+lane+1);
        return result.end>=result.first ? result : Window{};
    }
};
} // namespace openosc::scope
