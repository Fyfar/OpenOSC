#pragma once
#include <cstdint>

namespace openosc::scope {
class TriggerReference {
public:
    static uint16_t dacCode(int16_t signedLevel) {
        // Factory comparator mapping: code=(128-1.6*Y)*0.824.
        // Retain fractional Y when interpolating the measured PA4 reference table.
        constexpr int scale=824, step=20*scale;
        int y=80*scale-int(signedLevel)*625;
        if (y<0) y=0;
        if (y>160*scale) y=160*scale;
        constexpr int millivolts[]={1570,1480,1360,1275,1170,1025,950,830,750};
        const unsigned interval=y>=160*scale ? 7 : y/step;
        const int voltage=millivolts[interval]+
            (millivolts[interval+1]-millivolts[interval])*(y-int(interval)*step)/step;
        return uint16_t(voltage*4096/2500);
    }
};
} // namespace openosc::scope
