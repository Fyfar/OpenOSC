#pragma once
#include "openosc/data/status.hpp"
#include <algorithm>
#include <cstdint>

namespace openosc::scope {
class TriggerDelayPlan {
public:
    struct Span {
        uint16_t prescaler=0,reload=0;
        uint64_t ticks() const { return (uint64_t(prescaler)+1)*(uint64_t(reload)+1); }
    };
    void begin(uint64_t ticks) { remaining_=ticks ? ticks : 1; }
    bool next(Span &span) {
        if (!remaining_) return false;
        const uint64_t chunk=std::min<uint64_t>(remaining_,1ULL<<32);
        const uint64_t prescaler=(chunk+65535)/65536;
        const uint64_t period=std::max<uint64_t>(2,(chunk+prescaler-1)/prescaler); // ARR=0 blocks these timer counters.
        span={uint16_t(prescaler-1),uint16_t(period-1)};
        remaining_-=chunk;
        return true;
    }
    static bool sampleTicks(uint32_t samples,Rational rate,uint32_t clock,uint64_t &output) {
        if (!rate.valid() || !clock) return false;
        const uint64_t period=uint64_t(clock)*rate.denominator;
        const uint64_t whole=period/rate.numerator;
        const uint64_t extra=(uint64_t(samples)*(period%rate.numerator)+rate.numerator-1)/rate.numerator;
        if (samples && whole>(UINT64_MAX-extra)/samples) return false;
        output=uint64_t(samples)*whole+extra;
        return true;
    }
private:
    uint64_t remaining_=0;
};
} // namespace openosc::scope
