#include "openosc/instrument/data_access.hpp"
#include "openosc/calibration/record.hpp"
#include "openosc/acquisition/sample_plan.hpp"
#include "openosc/generator/control_bits.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/platform/interrupts.hpp"
#include <algorithm>

namespace openosc {
namespace {
// The ZT703S DAC stream announces its start; the ZT706S module (UART, no DMA) has nothing to announce.
void attachBus(generator::Output &output, generator::BusClient &client) { output.attach(client); }
void attachBus(generator::ModuleOutput &, generator::BusClient &) {}
} // namespace
DataAccess::DataAccess(Filesystem &fs)
    : resources_(PeripheralResources::StorageDma), zeroFile_(fs),pwmFile_(fs,calibration_),gainFile_(fs,calibration_),interleaveFile_(fs,calibration_),zeroOperation_(*this), frontend_(scopeCalibration_), capture_(resources_, frontend_), counters_(resources_), uart_(resources_),
      generator_(resources_), serial_(resources_), recorder_(fs), logger_(fs) {
    attachBus(generator_, *this);
    StorageAdmission::attach(this); // NOR SPI transfers ask the capture to yield first.
}
bool DataAccess::initialize(MeasurementMode view) {
    CalibrationRecord record;
    record.load(calibration_);
    if (calibration_.valid()) {
        zeroFile_.load(calibration_.crc());
        if (pwmFile_.load()) scopeCalibration_.setPwm(pwmFile_.values());
        if (gainFile_.load()) scopeCalibration_.setGain(gainFile_.values());
        if (interleaveFile_.load()) scopeCalibration_.setInterleave(interleaveFile_.values());
    }
    previousTick_ = Board::milliseconds();
    timeUs_ = uint64_t(previousTick_) * 1000;
    if (!generator_.configure(generator::Settings{})) {
        status_ = DataStatus::HardwareError;
        return false;
    }
    generator_.disable(); // A known OFF state (the ZT706S module may still play what it had).
    initialized_ = true;
    return select(view == MeasurementMode::Generator ? MeasurementMode::Oscilloscope : view);
}
bool DataAccess::select(MeasurementMode view) {
    releaseReadout(); // Revoked first: nothing waits for a background readout.
    if (zeroBusy()) return false;
    if (!initialized_ || stopping_ || unsigned(view) >= InstrumentModeCount)
        return false;
    if (view == MeasurementMode::Generator)
        return true;
    if (active_ && view == measurement_)
        return true;
    if (!serial_.stop()) { status_=DataStatus::HardwareError; return false; }
    if (uart_.txStatus() == DataStatus::Busy) {
        status_ = DataStatus::Busy;
        return false;
    }
    if (!stopCapture(true)) {
        status_ = DataStatus::HardwareError;
        return false;
    }
    if (!uart_.stop()) {
        status_ = DataStatus::HardwareError;
        return false;
    }
    counters_.stop();
    const auto previous = measurement_;
    const bool restore = active_;
    active_ = false;
    if (activate(view))
        return true;
    const auto failure = status_;
    if (restore && previous != view) {
        if (!uart_.stop() || !stopCapture(true) || !activate(previous))
            status_ = DataStatus::HardwareError;
        else
            status_ = failure;
    }
    return false;
}
bool DataAccess::activate(MeasurementMode view) {
    active_ = false;
    meter_.cancelHold(); // A mode's cycle belongs to the DMM screen it was armed on.
    dmmSelector_.cancel();
    dmmKey_.cancel(); dmmRange_.cancel(); dmmOperation_ = DmmOperation::Function;
    framer_.reset();
    captureFailed_ = false;
    measurement_ = view;
    if (view == MeasurementMode::Multimeter) {
        dmmActivatedAtUs_ = timeUs_;
        meterGeneratorBits_ = generator::controlBits(generator_.offConfirmed());
        frontend_.multimeter(meterGeneratorBits_);
        if (!uart_.start()) {
            status_ = uart_.status();
            return false;
        }
        receiveErrors_ = uart_.errors();
        receiveOverflows_ = uart_.overflows();
        active_ = true;
        status_ = DataStatus::NoData;
        return true;
    }
    return activateScope();
}
bool DataAccess::generatorLoadsBus() const {
    // The ZT703S DAC stream, also while it announces its start; the ZT706S module's UART never.
    return generator_.loadsBus() || busAnnounced_;
}
bool DataAccess::findTrigger(const scope::FrameView &frame,uint32_t &index) {
    if (frame.metadata().rolling) return false;
    if (frame.metadata().triggered) { index=frame.metadata().triggerIndex; return true; }
    return triggerCache_.find(frame,request_.trigger,index);
}
bool DataAccess::acquireScope(scope::FrameLease &lease, bool consume) {
    if (zeroBusy() || zeroBlocked_) return false;
    if (!store_.acquireLatest(lease, consume))
        return false;
    if (lease.view().metadata().rolling) { lease.annotateTrigger(false,0); return true; }
    if (!scopeHeld_ && request_.trigger.mode == scope::TriggerMode::Single &&
        int32_t(lease.view().metadata().sequence - armSequence_) < 0) {
        lease.release(); return false;
    }
    const uint32_t sequence = lease.view().metadata().sequence;
    uint32_t trigger = 0;
    bool found = false;
    if (!scopeHeld_ || !heldTrigger_.recall(sequence, found, trigger)) {
        found = findTrigger(lease.view(), trigger);
        if (scopeHeld_) heldTrigger_.remember(sequence, found, trigger);
    }
    lease.annotateTrigger(found, trigger);
    if (!scopeHeld_ && !triggerAccepts(lease.view().metadata(), found)) {
        lease.release();
        return false;
    }
    return true;
}
const scope::FrameView *DataAccess::readoutFrame(uint32_t sequence) {
    if (readout_.valid() && readout_.view().metadata().sequence == sequence) return &readout_.view();
    releaseReadout();
    scope::FrameLease lease;
    if (!acquireScope(lease) || lease.view().metadata().sequence != sequence) return nullptr;
    readout_ = std::move(lease);
    return &readout_.view();
}
void DataAccess::releaseReadout() {
    if (!readout_.valid()) return;
    readout_.release();
    ++readoutRevision_;
}
bool DataAccess::acquireScopeWait(scope::FrameLease &lease,uint32_t timeoutMs) {
    const uint32_t start=Board::milliseconds();
    // Frames behind an overlay are old, so a paused capture has to produce a new one first.
    const bool stale=capturePaused_ && !scopeHeld_;
    const uint32_t firstFresh=sequence_+1;
    noteFrameDemand(start);
    // Re-check HOLD each pass: a Single that completes during the wait is the frame to return.
    while (!acquireScope(lease) || (stale && !scopeHeld_ && int32_t(lease.view().metadata().sequence-firstFresh)<0)) {
        lease.release();
        const uint32_t now=Board::milliseconds();
        if (uint32_t(now-start)>=timeoutMs) return false;
        poll(now);
        Board::service();
    }
    return true;
}
bool DataAccess::sendDmmGroup(dmm::CommandGroup group) {
    if (unsigned(group) >= 5) return false;
    return selectDmmFunction(dmm::Functions::next(group, dmm::Functions::identify(meter_.latest())));
}
bool DataAccess::selectDmmFunction(dmm::Function function) {
    if (!active_ || measurement_ != MeasurementMode::Multimeter || dmmKey_.busy() || dmmRange_.busy()) return false;
    if (!dmmSelector_.request(function, meter_.latest(), Board::milliseconds(), dmmActivatedAtUs_)) return false;
    // Accepted (a refusal above changes nothing): the statistics period starts with this function.
    meter_.restart(function, timeUs_ + uint64_t(dmm::FunctionSelector::DeadlineMs) * 1000);
    dmmOperation_ = DmmOperation::Function; return true;
}
bool DataAccess::advanceDmmRange() {
    return requestDmmKey(dmm::KeyCommand::Action::NextRange);
}
bool DataAccess::requestDmmKey(dmm::KeyCommand::Action action) {
    if (!active_ || measurement_ != MeasurementMode::Multimeter || dmmSelector_.busy() || dmmRange_.busy()) return false;
    if (!dmmKey_.request(action, meter_.latest(), Board::milliseconds(), dmmActivatedAtUs_)) return false;
    using Action = dmm::KeyCommand::Action;
    if (action == Action::NextRange || action == Action::Auto || action == Action::Manual) meter_.rangeCommand();
    dmmOperation_ = DmmOperation::Key; return true;
}
bool DataAccess::setDmmAuto(bool automatic) {
    return requestDmmKey(automatic ? dmm::KeyCommand::Action::Auto : dmm::KeyCommand::Action::Manual);
}
bool DataAccess::holdMeter(bool held) {
    if (!active_ || measurement_ != MeasurementMode::Multimeter) return false;
    if (!held && meter_.latest().packetHold && !requestDmmKey(dmm::KeyCommand::Action::ReleaseHold)) return false;
    if (!held) meter_.cancelHold();
    return meter_.hold(held);
}
bool DataAccess::armMeterHold() {
    if (!active_ || measurement_ != MeasurementMode::Multimeter || meter_.userHeld()) return false;
    return meter_.armHold(timeUs_);
}
bool DataAccess::relativeMeter(bool enabled) {
    if (!active_ || measurement_ != MeasurementMode::Multimeter) return false;
    if (meter_.latest().peerRelative) {
        if (enabled) return true;
        if (!requestDmmKey(dmm::KeyCommand::Action::RelativeOff)) return false;
    }
    if (enabled && meter_.status(timeUs_, 2000000) == DataStatus::Stale) return false;
    return meter_.relative(enabled);
}
bool DataAccess::setDmmRange(unsigned index) {
    if (!active_ || measurement_ != MeasurementMode::Multimeter || dmmSelector_.busy() || dmmKey_.busy() ||
        meter_.latest().timestampUs < dmmActivatedAtUs_) return false;
    if (!dmmRange_.request(index, meter_.latest(), Board::milliseconds())) return false;
    meter_.rangeCommand(); // A new scale: a hold mode's window starts again.
    dmmOperation_ = DmmOperation::Range; return true;
}
bool DataAccess::stepDmmRange(int direction) {
    unsigned index, count;
    const auto &result = meter_.latest();
    if (!dmm::Ranges::current(result, &index)) return false;
    dmm::Ranges::list(dmm::Functions::identify(result), count);
    if (direction > 0 && index + 1 < count) ++index;
    if (direction < 0 && index) --index;
    return setDmmRange(index);
}
bool DataAccess::recordScope(ScopeRecorder::Format format) {
    releaseReadout(); // Revoked first: nothing waits for a background readout.
    if (zeroBusy()) return false;
    if (format != ScopeRecorder::Format::Raw && format != ScopeRecorder::Format::Csv)
        return false;
    if (recordPending_ || recorder_.busy() || (!scopeHeld_ && measurement_ != MeasurementMode::Oscilloscope))
        return false;
    recordPending_ = true;
    recordDeadlineUs_ = timeUs_ + 2000000;
    recordStatus_ = DataStatus::Busy;
    recordFormat_ = format;
    return true;
}
void DataAccess::beginShutdown() {
    releaseReadout(); // Revoked first: nothing waits for a background readout.
    if (stopping_)
        return;
    if (const auto *original=zeroOperation_.abandon()) {
        request_=*original; scopeHeld_=false; zeroBlocked_=true; zeroMinimumSequence_=sequence_+1;
    }
    stopping_ = true;
    dmmSelector_.cancel(); dmmKey_.cancel(); dmmRange_.cancel();
    generator_.disable();
    serial_.stop();
    counters_.stop();
    stopCapture(true);
    uart_.stop();
    heldFrame_.release();
    recordPending_ = false;
    logger_.finish();
}
bool DataAccess::resume() {
    if (!stopping_) return true;
    stopping_ = false;
    return activate(measurement_);
}
} // namespace openosc
