#include "openosc/application/controller.hpp"
#include "openosc/application/meter_range_memory.hpp"

namespace openosc {
void ApplicationController::serviceInstruments(uint32_t now) {
    const auto selection = data_.dmmCommandStatus();
    const bool visible = uiReady_;
    if (serialWatched_ && !data_.meterSerial().enabled()) {
        if (visible && data_.meterSerial().status()!=DataStatus::Ok) view_.notify("Serial output failed",now);
        serialWatched_=false;
        if (visible) refreshMeterPanel();
    }
    if (selection != lastSelection_) {
        if (visible && (selection == DataStatus::Timeout || selection == DataStatus::HardwareError))
            view_.notify("Multimeter command was not confirmed", now);
        lastSelection_ = selection;
        if (visible) refreshMeterPanel();
    }
    if (data_.measurementMode() == MeasurementMode::Multimeter && data_.dmmSelectionStatus() == DataStatus::Ok && !data_.dmmBusy()) {
        const auto &result = data_.meter().latest();
        const auto function = dmm::Functions::identify(result);
        if (meterRestore_ && function != dmm::Function::Unknown) {
            meterRestore_ = false;
            if (MeterRangeMemory::restores(prefs_.meter, prefs_.meterFunction, function, result.range)) {
                const bool accepted = prefs_.meter.automatic ? data_.setDmmAuto(true) : data_.setDmmRange(prefs_.meter.range);
                if (!accepted && visible) view_.notify("Saved DMM range could not be applied",now);
                if (accepted) return;
            }
        }
        if (function != dmm::Function::Unknown && prefs_.meterFunction != unsigned(function)) {
            prefs_.meterFunction = state_.meter.function = unsigned(function); changed(now); dataRefresh_ = true;
            if (visible) refreshMeterPanel();
        }
        // The confirmed range into prefs; not after a failed command (the saved choice stays).
        if (data_.dmmCommandStatus() == DataStatus::Ok && MeterRangeMemory::mirror(prefs_.meter, function, result)) changed(now);
    }
    if (recordWatched_ && !data_.scopeRecording()) {
        const auto status = data_.scopeRecordStatus();
        if (visible) view_.notify(status == DataStatus::Ok ? data_.scopeRecorder().path() :
                     status == DataStatus::NoSpace ? "Not enough free space for this record" :
                     "Waveform save failed / no completed capture", now);
        recordWatched_ = false;
    }
    auto &logger = data_.meterLogger();
    if (logger.active()) loggerWatched_ = true;
    else if (loggerWatched_) {
        if (visible) view_.notify(logger.status() == DataStatus::Ok ? logger.path() : "CSV logging failed", now);
        loggerWatched_ = false;
        if (visible) refreshMeterPanel();
    }
}
} // namespace openosc
