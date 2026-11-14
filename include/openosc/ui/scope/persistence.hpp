#pragma once
#include "openosc/ui/scope/options.hpp"

namespace openosc::plot {
class Persistence {
public:
    void clear();
    void resize(unsigned width,unsigned height);
    void advance(uint32_t now, uint16_t duration);
    void line(unsigned channel,int x1,int y1,int x2,int y2);
    bool pixel(unsigned channel,unsigned x,unsigned y) const;
private:
    void mark(unsigned channel, unsigned x, unsigned y);
    // Four-bit expiry never shortens retention. Its upper bound is
    // 15*ceil(duration/14) ms; infinite persistence uses bitmaps.
    union Pixels {
        uint8_t finite[2][MaximumHeight*MaximumWidth/2];
        uint8_t infinite[3][MaximumHeight*MaximumWidth/8];
    } pixels_;
    unsigned width_=0,height_=0;
    uint32_t tickAt_=0;
    uint16_t duration_=0;
};
} // namespace openosc::plot
