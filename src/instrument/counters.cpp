#include "openosc/instrument/data_access.hpp"

namespace openosc {
scope::CounterSnapshot DataAccess::scopeCounters() const {
    auto result=counters_.snapshot();
    result.enabled=active_ && !stopping_ && !zeroBusy() && !zeroBlocked_ && measurement_==MeasurementMode::Oscilloscope
        ? unsigned(request_.frontend.enabled[0])|(unsigned(request_.frontend.enabled[1])<<1) : 0;
    for (unsigned c=0;c<2;++c) if (!(result.enabled&(1U<<c))) result.channels[c].valid=false;
    return result;
}
} // namespace openosc
