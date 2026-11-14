#pragma once
#include "openosc/data/status.hpp"

namespace openosc::scope {
struct CaptureProgress {
    uint64_t words=0; // Accepted packed CH1/CH2 words in this continuous record.
    uint32_t sequence=0;
    DataStatus status=DataStatus::NoData;
    bool active=false;
};
} // namespace openosc::scope
