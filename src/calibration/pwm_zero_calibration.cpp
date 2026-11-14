#include "openosc/calibration/pwm_zero_calibration.hpp"
#include "openosc/calibration/scope_calibration.hpp"
#include "openosc/calibration/pwm_file.hpp"
#include <cstdio>

namespace openosc::calibration {
bool PwmZeroCalibration::available() const { return file_.available() && calibration_.valid(); }
void PwmZeroCalibration::begin(const ScopeRequest &original,uint16_t factoryCrc,uint32_t now) {
    original_=original; result_={}; result_.factoryCrc=factoryCrc;
    range_=point_=0; totalDeadline_=now+TotalMs;
    state_=State::Settling;
    if (factoryCrc!=calibration_.factory().crc()) state_=State::Uncalibrated;
    else beginNode(now);
}
// Each node starts at the compare in force (the profile's, else the factory's) and the factory slope.
bool PwmZeroCalibration::beginNode(uint32_t now) {
    pointDeadline_=now+PointMs; unsettled_=0;
    for (unsigned c=0;c<2;++c) {
        uint16_t start=0; double slope=0;
        if (!calibration_.node(c,range_,point_,start) || !PwmGeometry::slope(calibration_.factory(),c,range_,point_,slope)) {
            state_=State::Uncalibrated; return false;
        }
        solvers_[c].begin(start,slope);
    }
    return true;
}
ScopeRequest PwmZeroCalibration::trial() {
    // Only a running search puts its compares in force (an Uncalibrated start ends at once).
    if (active()) for (unsigned c=0;c<2;++c) calibration_.setTrial(c,range_,point_,solvers_[c].compare());
    return ZeroSampler::trial(original_,range_,point_);
}
void PwmZeroCalibration::applied(uint32_t now) {
    sampler_.reset(range_,IterationSamples,now);
    if (active()) state_=State::Settling;
}
void PwmZeroCalibration::tick(uint32_t now) {
    if (!active()) return;
    if (int32_t(now-pointDeadline_)>=0 || int32_t(now-totalDeadline_)>=0) { state_=State::Timeout; return; }
    if (state_==State::Settling && sampler_.settled(now)) state_=State::Sampling;
}
bool PwmZeroCalibration::observe(const scope::FrameView &frame,uint32_t now) {
    tick(now);
    if (state_!=State::Sampling) return false;
    const auto taken=sampler_.take(frame,result_.factoryCrc);
    if (taken==ZeroSampler::Take::Uncalibrated) { state_=State::Uncalibrated; return false; }
    if (taken==ZeroSampler::Take::None) return false;
    if (!sampler_.quality(PwmProfile::MaximumShiftCodes)) { state_=State::InvalidInput; return false; }
    if (!sampler_.complete()) return false;
    if (sampler_.drift(0)>DriftCodes || sampler_.drift(1)>DriftCodes) { // Not settled yet: measure again.
        if (++unsettled_>MaximumUnsettled) state_=State::InvalidInput;
        return active();
    }
    bool done=true;
    for (unsigned c=0;c<2;++c) {
        const auto step=solvers_[c].update(sampler_.statistics(c).mean-sampler_.expected(c));
        if (step==PwmSolver::Step::Failed) { state_=State::InvalidInput; return false; }
        done=done && step==PwmSolver::Step::Converged;
    }
    if (!done) return true; // The next compares of this node.
    for (unsigned c=0;c<2;++c) result_.compare[c][range_][point_]=solvers_[c].best();
    if (++point_==PwmProfile::Points) { point_=0; ++range_; }
    if (range_<PwmProfile::Ranges) return beginNode(now);
    result_.enabled=true;
    state_=result_.valid(calibration_.factory()) ? State::Complete : State::InvalidInput;
    return false;
}
bool PwmZeroCalibration::commit() {
    if (state_!=State::Complete || !file_.save(result_)) return false;
    return calibration_.setPwm(result_);
}
void PwmZeroCalibration::finish() { calibration_.clearTrial(); }
unsigned PwmZeroCalibration::percent() const {
    if (state_==State::Complete) return 100;
    return unsigned((range_*PwmProfile::Points+point_)*100/(PwmProfile::Ranges*PwmProfile::Points));
}
void PwmZeroCalibration::detail(char *output,size_t capacity) const {
    std::snprintf(output,capacity,"%u/%u it %u",point_+1,PwmProfile::Points,
                  solvers_[0].iterations()>solvers_[1].iterations() ? solvers_[0].iterations()+1 : solvers_[1].iterations()+1);
}
} // namespace openosc::calibration
