#pragma once
#include "openosc/measurement/dmm/result.hpp"
#include "openosc/platform/resources.hpp"

namespace openosc::dmm {
class SerialOutput {
public:
    explicit SerialOutput(PeripheralResources &resources) : resources_(resources) {}
    ~SerialOutput() { stop(); }
    bool start(uint32_t sequence);
    bool stop();
    void service(const Result &result,uint32_t now);
    bool enabled() const { return lease_.valid(); }
    DataStatus status() const { return status_; }
private:
    PeripheralResources &resources_;
    PeripheralLease lease_;
    DataStatus status_=DataStatus::NoData;
    uint32_t sequence_=0,sentAt_=0;
    bool busy_=false,sent_=false;
};
} // namespace openosc::dmm
