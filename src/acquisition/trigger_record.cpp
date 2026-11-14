#include "openosc/acquisition/trigger_record.hpp"
#include "openosc/acquisition/record_depths.hpp"
#include <algorithm>

namespace openosc::scope {
FrameView TriggerRecord::select(const FrameView &ring,const TriggerSettings &settings,uint32_t count,uint32_t target,bool forced) {
    if (!ring.valid() || ring.metadata().rolling || !settings.valid() || count<RecordDepth::Minimum || count>RecordDepth::Maximum || ring.metadata().samples<4) return {};
    count=std::min(count,ring.metadata().samples);
    uint32_t index=0;
    const bool found=TriggerSearch::nearest(ring,settings,std::min(target,ring.metadata().samples-1),index);
    if (settings.mode!=TriggerMode::Auto && !found && !forced) return {};
    const uint32_t pre=uint64_t(count-1)*settings.position/1000;
    const uint32_t at=found ? index : std::min(target,ring.metadata().samples-1);
    const uint32_t first=found || forced ? std::min(at>pre ? at-pre : 0,ring.metadata().samples-count) : ring.metadata().samples-count;
    return ring.slice(first,count).withTrigger(found,found ? index-first : 0);
}
} // namespace openosc::scope
