#pragma once
#include "openosc/calibration/zero_procedure.hpp"
#include "openosc/calibration/zero_sampler.hpp"
#include "openosc/calibration/pwm_solver.hpp"
#include "openosc/calibration/pwm_profile.hpp"

namespace openosc::calibration {
class ScopeCalibration;
class PwmFile;
// The hardware zero: at every range and node (both channels together) the offset PWM compare is
// searched (PwmSolver) until a grounded input reads the zero code of the frame's own metadata, the
// target of the factory transfer. Each iteration sets the trial compares in the calibration
// overlay, waits for the analog path to settle (ZeroSampler: SettleMs, frames after it), takes
// IterationSamples per channel and steps both solvers. Only after all 2 x 11 x 7 nodes is the
// profile saved (scope_pwm.json) and then put in force; any failure keeps the profile in force
// and the file as they were. The target is absolute, so a repeated run finds the same compares
// rather than adding a correction again.
class PwmZeroCalibration final : public ZeroProcedure {
public:
    static constexpr double DriftCodes=1;    // The two halves of an iteration differ by no more.
    static constexpr unsigned MaximumUnsettled=3; // Re-measurements of a drifting iteration per node.
    // MeasurementMs: one measurement's allowance (SettleMs, the frame begun before it, IterationSamples
    // at 30k samples/s or more). A node may take every solver iteration and re-measurement; the run
    // is budgeted at ExpectedMeasurements per node (a noiseless node takes at most six, three at a slope
    // ratio of 1/2..2, plus a drift re-measurement).
    static constexpr uint32_t IterationSamples=32768,MeasurementMs=1250,ExpectedMeasurements=6,
        PointMs=(PwmSolver::MaximumIterations+MaximumUnsettled+1)*MeasurementMs,
        TotalMs=PwmProfile::Ranges*PwmProfile::Points*ExpectedMeasurements*MeasurementMs;
    PwmZeroCalibration(ScopeCalibration &calibration,PwmFile &file) : calibration_(calibration),file_(file) {}
    const char *title() const override { return "HARDWARE ZERO"; }
    const char *name() const override { return "Hardware zero"; }
    // No measurement tells a DC input from an offset within MaximumShiftCodes: the user grounds them.
    const char *requirement() const override { return "Keep both Scope inputs grounded"; }
    bool available() const override;
    uint32_t durationMs() const override { return TotalMs; }
    void begin(const ScopeRequest &original,uint16_t factoryCrc,uint32_t now) override;
    ScopeRequest trial() override;
    void applied(uint32_t now) override;
    bool observe(const scope::FrameView &frame,uint32_t now) override;
    void tick(uint32_t now) override;
    bool commit() override;
    void finish() override;
    unsigned percent() const override;
    void detail(char *output,size_t capacity) const override;
    uint32_t samples() const override { return sampler_.samples(); }
    const PwmProfile &result() const { return result_; }
    const PwmSolver &solver(unsigned channel) const { return solvers_[channel]; }
private:
    bool beginNode(uint32_t now);
    ScopeCalibration &calibration_;
    PwmFile &file_;
    ZeroSampler sampler_;
    PwmSolver solvers_[2];
    PwmProfile result_;
    uint32_t pointDeadline_=0,totalDeadline_=0;
    unsigned unsettled_=0;
};
} // namespace openosc::calibration
