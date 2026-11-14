#pragma once
#include <algorithm>
#include <cstdint>

namespace openosc::scope {
class DmaCycleGuard {
public:
    void begin(uint64_t period,uint32_t now) {
        period_=period && period<=INT32_MAX/2 ? uint32_t(period) : 0;
        for (auto &due:due_) due=now+period_;
    }
    bool observe(unsigned lane,uint32_t now) {
        if (lane>=4) return false;
        if (!period_) return true; // Slow rings exceed the useful 32-bit DWT interval.
        const int32_t late=int32_t(now-due_[lane]);
        due_[lane]+=period_;
        // TC is a single pending bit. Near the following wrap it no longer
        // distinguishes one completion from two. Allow normal IRQ latency,
        // keeping a small margin for the four lane phases and DMA visibility.
        const uint32_t margin=std::min(period_/2,std::max<uint32_t>(16,period_/1024));
        return late<int32_t(period_-margin);
    }
private:
    uint32_t period_=0,due_[4]{};
};
} // namespace openosc::scope
