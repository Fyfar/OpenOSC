#include "openosc/instrument/data_access.hpp"

// The capture's share of the AHB with the ZT703S DAC stream (Q-14). A streaming DAC halves the
// capture's maximum rate (captureRateLimit), but only plans made after it started know that.
// - Start: the stream announces itself before its first BDMA request (generator::BusClient). A
//   capture running above the rate its request now plans stops there, before any request of the
//   stream, and the next poll starts it again at the halved rate: no window with both at full speed.
//   Nothing failed: Normal and Single stay armed (the new capture is past the arm), a force carries
//   over, HOLD's pending wait and the record on show stay, the status is not touched.
// - End (disable, a generator fault, PWM in place of the DAC): no stop. The running capture keeps
//   its lower rate, which the bus allows; the next one plans the full rate.
// - Either way, when no capture runs and no zero calibration owns the plans, the Next plan
//   (ACQ:SRAT?, ACQ:MDEP?, the Next card) is described again; a running capture is what
//   configuration_ describes.
// ZT706S: the module (UART, no DMA) never loads the bus, so neither plan nor capture changes.
namespace openosc {
bool DataAccess::dacStreamStarting() {
    busAnnounced_ = true; // generatorLoadsBus(): the stream starts as soon as this returns.
    const bool yielded = yieldBus();
    busAnnounced_ = false;
    if (yielded) busLoad_ = true;
    return yielded;
}
// Each poll: a load that changed without an announcement (the stream ended) is followed here.
void DataAccess::followBusLoad() {
    const bool load = generatorLoadsBus();
    if (load == busLoad_) return;
    busLoad_ = load;
    if (!yieldBus()) status_ = capture_.status(); // A capture that would not stop says why.
}
bool DataAccess::yieldBus() {
    if (capture_.state() == scope::FrameMailbox::State::Capturing && !withinBusLimit() && !stopCapture(false))
        return false;
    // A zero calibration describes its own plans (its trial, then the original's restoration).
    if (capture_.state() == scope::FrameMailbox::State::Idle && !zeroBusy() && !describeNext(configuration_))
        describeChannels(configuration_);
    return true;
}
// The running capture's point rate against the rate its request plans under the bus load now. The
// plan keeps within the loaded maximum, and the capture timer never samples faster than its plan
// (CaptureTiming), so an unchanged plan never restarts and a capture within it is within the bus.
// A capture of a staged request (HOLD's pending record) is not the request's: it yields.
bool DataAccess::withinBusLimit() const {
    if (staged_) return false;
    scope::SamplePlan plan;
    if (!samplePlan(plan)) return false;
    const auto &running = capture_.metadata().samplesPerSecond;
    return uint64_t(running.numerator) * plan.rate.denominator <= uint64_t(plan.rate.numerator) * running.denominator;
}
} // namespace openosc
