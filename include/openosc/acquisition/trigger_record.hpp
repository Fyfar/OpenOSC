#pragma once
#include "openosc/acquisition/analysis.hpp"

namespace openosc::scope {
class TriggerRecord {
public:
    // The record around the edge nearest `target` (the comparator event). Without one: Auto takes the
    // newest `count` points, a forced capture the record placed at `target` as an edge would be (it is
    // not marked triggered), Normal/Single nothing.
    static FrameView select(const FrameView &ring,const TriggerSettings &settings,uint32_t count,uint32_t target,bool forced=false);
};
} // namespace openosc::scope
