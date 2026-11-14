#pragma once
#include "openosc/measurement/dmm/result.hpp"
#include "openosc/platform/resources.hpp"

namespace openosc::dmm {
// Stands in for SerialOutput on boards with no pin for the diagnostic stream.
class NoSerialOutput {
public:
    explicit NoSerialOutput(PeripheralResources &) {}
    bool start(uint32_t) { return false; }
    bool stop() { return true; }
    void service(const Result &, uint32_t) {}
    bool enabled() const { return false; }
    DataStatus status() const { return DataStatus::Unsupported; }
};
} // namespace openosc::dmm
