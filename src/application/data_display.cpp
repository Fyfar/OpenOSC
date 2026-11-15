#include "openosc/application/controller.hpp"

namespace openosc {
// The level belongs to the next capture; its marker sits on the scale of the record on screen.
void ApplicationController::showTrigger(const scope::FrameMetadata *shown) {
    const auto &trigger = data_.scopeRequest().trigger;
    const unsigned source = unsigned(trigger.channel);
    const auto &settings = data_.scopeMetadata().channels[source];
    view_.waveform().trigger(trigger.channel, shown ? scope::TriggerLevel::inFrame(trigger.level, settings, shown->channels[source]) : double(trigger.level));
    view_.scopeTrigger(trigger, settings);
}
void ApplicationController::displayData(uint32_t now) {
    if (uint32_t(now-fpsAt_)>=1000) {
        const uint32_t flushes=display_.flushes();
        view_.fps(prefs_.showFps,(flushes-fpsFlushes_)*1000/uint32_t(now-fpsAt_));
        fpsFlushes_=flushes; fpsAt_=now;
    }
    view_.generatorActive(data_.generator().enabled());
    // The DAC stream halves the capture's maximum rate: said once as it starts (output on, PWM to DAC,
    // a Dedicated square below 1 kHz, a startup restore); the Scope's GEN note keeps ", ADC max /2".
    const bool captureHalved = data_.generatorLoadsBus();
    if (captureHalved && !captureHalvedNoticed_ && uiReady_) view_.notify("GEN on DAC: Scope max sample rate /2 while it runs", now);
    captureHalvedNoticed_ = captureHalved;
    if (state_.mode == MeasurementMode::Oscilloscope)
        view_.scopeTriggerState(data_.scopeRequest().trigger.mode, data_.scopeWaiting());
    if (state_.mode==MeasurementMode::Oscilloscope) {
        view_.scopeCounters(data_.scopeCounters());
        view_.scopeAutoSetup(autoBusy());
        view_.scopeCalibrating(data_.zeroBusy());
    }
    if (state_.mode == MeasurementMode::Multimeter && !state_.settingsOpen && panel_ == Panel::None &&
        uint32_t(now - meterBarAt_) >= 8) {
        meterBarAt_ = now;
        const auto &meter = data_.meter();
        view_.multimeterBar(meter.displayed(), meter.status(data_.nowUs(),2000000), &meter.relative(), now);
    }
    if (state_.mode != MeasurementMode::Oscilloscope) readouts_.reset(data_); // Left the Scope: nothing kept.
    const unsigned interval = state_.mode == MeasurementMode::Oscilloscope && panel_ == Panel::None ? 16 : 40;
    if (uint32_t(now - dataAt_) < interval) return;
    if (state_.settingsOpen || panel_==Panel::Images || panel_==Panel::Zero) {
        readouts_.suspend(data_); // Not serviced while this lasts: no background lease is kept.
        dataAt_ = now;
        if (state_.mode == MeasurementMode::Oscilloscope) view_.captureStatus(data_.status(), data_.scopeHeld());
        else if (state_.mode == MeasurementMode::Multimeter) {
            const auto &meter = data_.meter();
            view_.multimeterStatus(meter.displayed(), meter.held(), meter.status(data_.nowUs(), 2000000), meter.relative().active());
        } else view_.generatorStatus(data_.generator().snapshot());
        return;
    }
    if (state_.mode == MeasurementMode::Oscilloscope) {
        auto options=view_.waveform().options();
        if (heldViewActive_ && data_.scopeHoldReleases()!=heldViewRelease_) { // HOLD explores one record.
            options.restoreHeldView(heldView_);
            view_.waveform().configure(options); heldViewActive_=false; dataRefresh_=true;
        }
        followNextTimebase(); // Whoever staged it (keys, Timebase menu, a host): one place.
        options=view_.waveform().options();
        // The next capture's settings do not change what a held record contains.
        if (!channelShown(options.spectrum.channel)) {
            options.spectrum.channel ^= 1;
            view_.waveform().configure(options);
        }
        if (options.cursors.voltage && !channelShown(options.cursors.channel) && channelShown(options.cursors.channel^1)) {
            auto cursors=options.cursors; cursors.channel^=1;
            view_.waveform().cursorLines(cursors); queuePlotSave(now);
        }
        followShownChannel();
        if (options.mode==plot::Mode::XY && (!channelShown(0) || !channelShown(1))) {
            options.mode=plot::Mode::Time; view_.waveform().configure(options);
            view_.notify("XY needs CH1 + CH2; Y-T active",now);
        }
        if (dataRefresh_) {
            auto next = data_.scopeMetadata();
            const auto &request = data_.scopeRequest();
            next.timebaseNs = request.timebaseNs; // The metadata keeps the running capture's until it restarts.
            const auto &model = view_.waveform().model();
            showTrigger(model.hasFrame() ? &model.metadata() : nullptr);
            view_.scopeContext(selectedChannel_,focus_);
            view_.scopeSettings(next,selectedChannel_);
            syncCursorFocus();
        }
        view_.scopeGenerator(data_.generator().snapshot(), captureHalved);
        if (!view_.waveformVisible()) { readouts_.suspend(data_); dataAt_=now; return; }
        scope::FrameLease frame;
        if (data_.acquireScope(frame)) {
            const auto sequence = frame.view().metadata().sequence;
            if (dataRefresh_ || lastScopeSequence_ != sequence || lastScopeHold_ != data_.scopeHeld() ||
                lastScopeStatus_ != data_.status() || view_.waveform().needsFrame()) {
                dataAt_ = now;
                showTrigger(&frame.view().metadata());
                view_.capture(frame.view(), data_.scopeHeld(), selectedChannel_, data_.status());
                lastScopeSequence_ = sequence;
                lastScopeHold_ = data_.scopeHeld();
                lastScopeStatus_ = data_.status();
            } else if (data_.scopeHeld())
                dataAt_ = now;
        } else if (dataRefresh_ || lastScopeStatus_ != data_.status()) {
            view_.captureStatus(data_.status(), data_.scopeHeld());
            lastScopeStatus_ = data_.status();
        }
        readouts_.service(data_, view_, state_.scope.measurements, now);
    } else if (state_.mode == MeasurementMode::Multimeter) {
        dataAt_ = now;
        const auto &meter = data_.meter();
        const auto status = meter.status(data_.nowUs(), 2000000);
        if (dataRefresh_ || lastMeterSequence_ != meter.displayed().sequence ||
            lastMeterHold_ != meter.held() || lastMeterStatus_ != status) {
            const ui::MeterContext context{&meter.statistics(), &meter.history(), &meter.relative(), prefs_.meter.display,
                                          bool(prefs_.meter.statistics), prefs_.meter.face, prefs_.meter.palette,
                                          &prefs_.meter.appearance};
            view_.multimeter(meter.displayed(), meter.held(), status, context);
            lastMeterSequence_ = meter.displayed().sequence;
            lastMeterStatus_ = status;
            lastMeterHold_ = meter.held();
        }
    } else {
        dataAt_ = now;
        const auto output = data_.generator().snapshot();
        if (dataRefresh_ || output.enabled != lastGenerator_.enabled ||
            output.status != lastGenerator_.status) {
            view_.generator(output);
            lastGenerator_ = output;
        }
    }
    dataRefresh_ = false;
}
} // namespace openosc
