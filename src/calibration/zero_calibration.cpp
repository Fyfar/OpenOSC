#include "openosc/calibration/zero_calibration.hpp"
#include "openosc/calibration/scope_calibration.hpp"

namespace openosc::calibration {
bool ZeroCalibration::available() const { return file_.available(); }
bool ZeroCalibration::shadowed() const { return calibration_.pwmActive(); }
bool ZeroCalibration::commit() { return file_.save(result_); }
void ZeroCalibration::resetPoint(uint32_t now) {
    sampler_.reset(range_,SamplesPerPoint,now); rangeDeadline_=now+5000;
    state_=State::Settling;
}
void ZeroCalibration::begin(const ScopeRequest &original,uint16_t factoryCrc,uint32_t now) {
    original_=original; result_={}; result_.factoryCrc=factoryCrc;
    range_=point_=0; totalDeadline_=now+TotalMs;
    resetPoint(now);
}
void ZeroCalibration::tick(uint32_t now) {
    if (!active()) return;
    if (int32_t(now-rangeDeadline_)>=0 || int32_t(now-totalDeadline_)>=0) { state_=State::Timeout; return; }
    if (state_==State::Settling && sampler_.settled(now)) state_=State::Sampling;
}
unsigned ZeroCalibration::percent() const {
    if (state_==State::Complete) return 100;
    return unsigned(((uint64_t(range_)*ZeroProfile::Points+point_)*SamplesPerPoint+sampler_.samples())*100/
        (ZeroProfile::Ranges*ZeroProfile::Points*SamplesPerPoint));
}
bool ZeroCalibration::observe(const scope::FrameView &frame,uint32_t now) {
    tick(now);
    if (state_!=State::Sampling) return false;
    const auto taken=sampler_.take(frame,result_.factoryCrc);
    if (taken==ZeroSampler::Take::Uncalibrated) { state_=State::Uncalibrated; return false; }
    if (taken==ZeroSampler::Take::None) return false;
    if (!sampler_.quality(ZeroProfile::MaximumCorrection)) { state_=State::InvalidInput; return false; }
    if (!sampler_.complete()) return false;
    for (unsigned c=0;c<2;++c) result_.codes[c][range_][point_]=float(sampler_.statistics(c).mean-sampler_.expected(c));
    if (++point_==ZeroProfile::Points) { point_=0; ++range_; }
    if (range_==ZeroProfile::Ranges) {
        state_=result_.valid() ? State::Complete : State::InvalidInput;
        result_.enabled=state_==State::Complete;
    } else resetPoint(now);
    return true;
}
} // namespace openosc::calibration
