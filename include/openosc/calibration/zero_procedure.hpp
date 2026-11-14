#pragma once
#include "openosc/acquisition/request.hpp"
#include <cstddef>

namespace openosc::calibration {
// One zero calibration as ZeroOperation runs it (stop GEN, trials in the trial area, restore):
// the software zero (ZeroCalibration: a correction of the zero code) or the hardware zero
// (PwmZeroCalibration: the offset PWM itself). It measures range by range and node by node and
// keeps nothing in force until commit().
class ZeroProcedure {
public:
    enum class State { Idle,Settling,Sampling,Complete,InvalidInput,Uncalibrated,Timeout,Cancelled };
    virtual ~ZeroProcedure()=default;
    virtual const char *title() const=0; // The progress panel's: "HARDWARE ZERO".
    virtual const char *name() const=0;  // In messages: "Hardware zero".
    virtual const char *requirement() const=0; // What the inputs must be, on the progress panel.
    virtual bool available() const=0; // Its result can be saved (the calibration disk is there).
    // Its result would not be in force (the software zero under a hardware zero): refused at once.
    virtual bool shadowed() const { return false; }
    virtual uint32_t durationMs() const=0; // Its own deadline over all ranges and nodes.
    virtual void begin(const ScopeRequest &original,uint16_t factoryCrc,uint32_t now)=0;
    virtual ScopeRequest trial()=0;   // The request of the next trial (the hardware zero also sets its compares).
    virtual void applied(uint32_t at)=0; // trial() is at the hardware; its analog path changed at `at`.
    virtual bool observe(const scope::FrameView &frame,uint32_t now)=0; // True: a new trial is to be applied.
    virtual void tick(uint32_t now)=0;
    virtual bool commit()=0; // Complete: saved, then in force; false: nothing changed (IoError).
    virtual void finish() {} // Any end: no trial of it stays in force.
    virtual unsigned percent() const=0;
    // A short note of the step within a node (iteration), empty if none.
    virtual void detail(char *output,size_t capacity) const { if (capacity) *output=0; }
    virtual uint32_t samples() const=0;
    // Waiting for the user (the gain's reference, its review): no frame is due meanwhile; the
    // procedure keeps its own deadline.
    virtual bool waiting() const { return false; }
    // Why it ended InvalidInput, when it can say more than that ("failed: ...").
    virtual const char *reason() const { return nullptr; }
    void cancel() { if (active()) state_=State::Cancelled; }
    bool active() const { return state_==State::Settling || state_==State::Sampling; }
    State state() const { return state_; }
    unsigned range() const { return range_; }
    unsigned point() const { return point_; }
    const ScopeRequest &original() const { return original_; }
protected:
    ScopeRequest original_;
    State state_=State::Idle;
    unsigned range_=0,point_=0;
};
} // namespace openosc::calibration
