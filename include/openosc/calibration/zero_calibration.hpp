#pragma once
#include "openosc/calibration/zero_procedure.hpp"
#include "openosc/calibration/zero_sampler.hpp"

namespace openosc::calibration {
class ScopeCalibration;
// Where a software zero is kept (ZeroFile on the calibration disk).
class ZeroStore {
public:
    virtual ~ZeroStore()=default;
    virtual bool available() const=0;
    virtual bool save(const ZeroProfile &values)=0;
};
// The software zero: at every range and node the mean zero-input code is measured and its
// difference from the expected one saved (scope_zero.json); captures then add it to their zero
// code. The offset PWM stays the factory's. Under a hardware zero (ScopeCalibration::pwmActive)
// it is shadowed: a capture adds none of it, and a measurement under the profile's PWM would only
// overwrite the kept legacy values with ~0, so it is refused before anything changes.
class ZeroCalibration final : public ZeroProcedure {
public:
    // Up to one sample in 2048 may be an isolated bus glitch; it never enters the zero.
    static constexpr uint32_t SamplesPerPoint=262144,SettleMs=ZeroSampler::SettleMs,OutlierDivisor=ZeroSampler::OutlierDivisor,TotalMs=180000;
    ZeroCalibration(ZeroStore &file,const ScopeCalibration &calibration) : file_(file),calibration_(calibration) {}
    const char *title() const override { return "SOFTWARE ZERO"; }
    const char *name() const override { return "Software zero"; }
    const char *requirement() const override { return "Keep both Scope inputs disconnected"; }
    bool available() const override;
    bool shadowed() const override;
    uint32_t durationMs() const override { return TotalMs; }
    void begin(const ScopeRequest &original,uint16_t factoryCrc,uint32_t now) override;
    ScopeRequest trial() override { return ZeroSampler::trial(original_,range_,point_); }
    void applied(uint32_t now) override { resetPoint(now); }
    bool observe(const scope::FrameView &frame,uint32_t now) override; // True after advancing a calibration point.
    void tick(uint32_t now) override;
    bool commit() override;
    unsigned percent() const override;
    uint32_t samples() const override { return sampler_.samples(); }
    const ZeroProfile &result() const { return result_; }
private:
    void resetPoint(uint32_t now);
    ZeroStore &file_;
    const ScopeCalibration &calibration_;
    ZeroSampler sampler_;
    ZeroProfile result_;
    uint32_t rangeDeadline_=0,totalDeadline_=0;
};
} // namespace openosc::calibration
