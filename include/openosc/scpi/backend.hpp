#pragma once
#include "openosc/acquisition/request.hpp"
#include "openosc/acquisition/counter_window.hpp"
#include "openosc/acquisition/progress.hpp"
#include "openosc/generator/waveform.hpp"
#include "openosc/measurement/dmm/ranges.hpp"
#include "openosc/measurement/dmm/session.hpp"
#include "openosc/instrument/mode.hpp"
#include "openosc/data/performance.hpp"

namespace openosc::scpi {
// Called only by the foreground dispatcher. A frame remains leased until releaseFrame().
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool multimeter() const = 0;
    virtual const ScopeRequest &scopeRequest() const = 0;
    virtual const scope::FrameMetadata *configuration() const = 0;
    virtual bool configureScope(const ScopeRequest &) = 0;
    virtual bool run(bool single) = 0;
    virtual bool autoScale() { return false; }
    // A forced trigger (TFORce): a record without a trigger event (DataAccess::forceScope). False:
    // refused (not Scope, HOLD or a done Single, zero/AutoSetup busy, shutdown): -221.
    virtual bool forceTrigger() { return false; }
    // The sequence of the newest frame a reader is given (finite or Roll), 0 for none; takes no lease.
    virtual uint32_t frameSequence() const { return 0; }
    virtual void stop() = 0;
    virtual bool held() const = 0;
    virtual bool waiting() const = 0;
    virtual const scope::FrameView *frame(bool refresh) = 0;
    virtual void releaseFrame() = 0;
    // The session ended (Engine::reset: a new session, a disconnect, a clear): its frame lease and
    // any claim on records it has not read yet go.
    virtual void endSession() { releaseFrame(); }
    virtual const dmm::Result &meter() const = 0;
    virtual DataStatus meterStatus() const = 0;
    virtual bool selectFunction(dmm::Function) = 0;
    virtual bool setAuto(bool) = 0;
    virtual bool setRange(unsigned) = 0;
    // The panel's HOLD and REL, and the statistics of the DMM screen (Session: one owner, the same
    // period the screen shows). meter() already reports both states (packetHold, peerRelative).
    virtual bool holdMeter(bool) = 0;
    virtual bool relativeMeter(bool) = 0;
    virtual dmm::StatisticsView meterStatistics() const = 0; // As the DMM screen shows it.
    virtual void resetMeterStatistics() = 0;
    // The MODE screen, as the MODE key selects it. A different measurement (Scope <-> DMM) changes the
    // USB profile: the device reconnects, as after MODE on the panel.
    virtual MeasurementMode screen() const = 0;
    virtual bool screen(MeasurementMode) = 0;
    virtual bool busy() const = 0;
    virtual uint32_t operationTimeoutMs() const { return 10000; }
    virtual DataStatus operationStatus() const = 0;
    virtual generator::Snapshot generator() const = 0;
    virtual bool configureGenerator(const generator::Settings &) = 0;
    virtual bool output(bool enabled) = 0;
    virtual bool remote() const = 0;
    virtual bool remote(bool enabled) = 0;
    virtual bool performance(PerformanceSnapshot &) const { return false; }
    virtual bool recovery() { return false; }
    // Run mode as the core clock in MHz (0: no profile runs, the HSI fallback). The setter changes
    // it in place, as the Settings menu does, and returns 0 or the SCPI error to report.
    virtual uint32_t coreMhz() const { return 0; }
    virtual int coreMhz(uint32_t) { return -241; }
    virtual bool counter(unsigned,scope::CounterReading &) const { return false; }
    virtual scope::CaptureProgress captureProgress() const { return {}; }
    virtual const char *serial() const = 0;
    virtual const char *version() const = 0;
};
} // namespace openosc::scpi
