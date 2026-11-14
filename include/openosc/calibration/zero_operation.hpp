#pragma once
#include "openosc/calibration/zero_procedure.hpp"
#include "openosc/acquisition/mailbox.hpp"

namespace openosc::calibration {
class ZeroInstrument {
public:
    virtual ~ZeroInstrument()=default;
    virtual DataStatus zeroAvailable() const=0;
    virtual const ScopeRequest &zeroRequest() const=0;
    virtual uint16_t zeroFactoryCrc() const=0;
    virtual bool zeroHeld() const=0;
    virtual void zeroHold(bool hold)=0;
    virtual bool zeroStopGenerator()=0;
    virtual bool zeroConfigure(const ScopeRequest &request)=0;
    // After zeroConfigure: false until its settings are at the hardware (a capture start may apply
    // them later), then `at` = when the analog path last changed, the origin of its settle time.
    virtual bool zeroAppliedAt(uint32_t &at) const=0;
    virtual uint32_t zeroSequence() const=0;
    virtual bool zeroAcquire(scope::FrameLease &frame)=0;
    virtual void zeroRestoreFailed(const ScopeRequest &original)=0;
    // HOLD: the record on show (raw, metadata, view) is kept aside, untouched, while the trials run
    // in the trial area (AutoSetup's retain); false: it cannot be (a reader has it), nothing changed.
    virtual bool zeroRetain()=0;
    // Back to HOLD with that very record and the original request (the front end at the offsets in
    // force now: only the next RUN captures with them).
    virtual bool zeroRestoreHeld(const ScopeRequest &original)=0;
    virtual void zeroReleaseRetained()=0; // The kept record is not wanted (RUN chosen, abandoned).
};
class ZeroOperation {
public:
    enum class Phase { Idle,Applying,Collecting,Restoring,Resuming };
    enum class Result { None,Busy,Complete,Cancelled,InvalidInput,Uncalibrated,Timeout,IoError,HardwareError,RestoreFailed,Shadowed };
    explicit ZeroOperation(ZeroInstrument &instrument) : instrument_(instrument) {}
    // Runs `procedure` (the software or the hardware zero) until it ends and the capture is back.
    bool begin(uint32_t now,ZeroProcedure &procedure);
    void service(uint32_t now);
    void cancel(uint32_t now);
    void holdAfter(bool hold,uint32_t now);
    // A terminating firmware session needs no hardware restoration. Its owner
    // restores the request before shutdown/resume and closes the actual drivers.
    const ScopeRequest *abandon();
    bool busy() const { return phase_!=Phase::Idle; }
    Phase phase() const { return phase_; }
    // Applying, its trial configured: the capture start that applies it (perhaps deferred, a burst
    // or memory still held) must run; before the configure no capture may start.
    bool trialConfigured() const { return phase_==Phase::Applying && configured_; }
    Result result() const { return result_; }
    // The procedure of the operation running or last run.
    const ZeroProcedure &calibration() const { return *procedure_; }
    bool hasRun() const { return procedure_!=nullptr; }
private:
    void restore(uint32_t now);
    void collecting(uint32_t now);
    ZeroInstrument &instrument_;
    ZeroProcedure *procedure_=nullptr;
    Phase phase_=Phase::Idle;
    Result result_=Result::None;
    uint32_t deadline_=0,resumeSequence_=0;
    bool holdAfter_=false,waitForFrame_=true,configured_=false; // configured_: the trial waits for the hardware.
    bool retained_=false; // The original HOLD record is kept aside (zeroRetain).
};
} // namespace openosc::calibration
