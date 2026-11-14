#pragma once
#include <cstdint>

namespace openosc::calibration {
struct ZeroProfile {
    static constexpr unsigned Channels=2,Ranges=11,Points=7;
    static constexpr int Positions[Points]={20,40,60,80,100,120,140};
    static constexpr double MaximumCorrection=16;
    uint16_t factoryCrc=0;
    bool enabled=false;
    float codes[Channels][Ranges][Points]{};
    bool valid() const;
    double correction(unsigned channel,unsigned range,int position=80) const;
};
} // namespace openosc::calibration
