#pragma once
#include <algorithm>
#include <cstdint>

namespace openosc::scope {
class LinearCopyPlan {
public:
    struct Span { uint32_t source=0,destination=0; uint16_t count=0; };
    bool begin(uint32_t origin,uint32_t ring,uint32_t count) {
        if (!ring || ring>65536 || origin>=ring || !count || count>ring) return false;
        origin_=origin; ring_=ring; count_=count; offset_=0; return true;
    }
    bool next(Span &span) {
        if (offset_==count_) return false;
        span.source=(origin_+offset_)%ring_; span.destination=offset_;
        span.count=uint16_t(std::min<uint32_t>({count_-offset_,ring_-span.source,65535}));
        offset_+=span.count; return true;
    }
private:
    uint32_t origin_=0,ring_=0,count_=0,offset_=0;
};
} // namespace openosc::scope
