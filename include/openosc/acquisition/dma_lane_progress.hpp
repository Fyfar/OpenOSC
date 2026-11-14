#pragma once
#include <cstdint>

namespace openosc::scope {
class DmaLaneProgress {
public:
    static uint64_t completed(uint64_t cycles,uint32_t remaining,bool pending,uint32_t words) {
        return (cycles+(pending && remaining ? 1 : 0))*words+words-remaining;
    }
    // NDTR can reload while the last already-requested transfer drains.
    // Do not read TC after disabling: software interruption itself sets TCIF.
    static uint32_t drained(uint32_t before,uint32_t after,uint32_t words) {
        return after<=before ? before-after : before+words-after;
    }
};
} // namespace openosc::scope
