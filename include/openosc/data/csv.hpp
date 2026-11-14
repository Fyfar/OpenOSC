#pragma once
#include "openosc/acquisition/frame.hpp"
#include "openosc/measurement/dmm/result.hpp"

namespace openosc {
class Csv {
public:
    static constexpr const char *DmmHeader =
        "timestamp_us,sequence,status,quantity,value_si,unit,display,packet_hold,user_hold,peer_relative\r\n";
    static constexpr const char *ScopeHeader = "index,time_seconds,ch1_code,ch2_code,ch1_volts,ch2_volts\r\n";
    static size_t dmm(const dmm::Result &result, bool userHold, char *output, size_t capacity);
    static size_t scope(const scope::FrameView &frame, size_t index, char *output, size_t capacity);
    static size_t scopeMetadata(const scope::FrameMetadata &metadata, char *output, size_t capacity);
};
} // namespace openosc
