#pragma once
#include <cstdint>

namespace openosc::power {
struct BatteryState {
    uint8_t bars=0;
    bool valid=false,low=false,external=false,full=false;
    uint16_t millivolts=0; // On the factory ADC scale, so an estimate rather than a calibrated reading.
    uint8_t percent=0;
    bool measured=false;
};

// The factory's ADC scale drives its six display bands. The voltage and percentage
// derived from it are estimates: the divider on PC4 has no independent calibration.
class BatteryGauge {
public:
    const BatteryState &state() const { return state_; }
    void digital(bool external,bool full) {
        const uint8_t bits=uint8_t(external)|(uint8_t(full)<<1);
        if (bits!=candidate_) { candidate_=bits; stable_=1; }
        else if (stable_<11) ++stable_;
        if (stable_==11) {
            if (state_.external!=external) resetSamples();
            state_.external=external; state_.full=external && full;
            if (external) { state_.low=false; state_.valid=true; state_.bars=full ? 5 : 0; state_.measured=false; sum_=0; averaged_=0; }
        }
    }
    void sample(uint16_t raw) {
        if (raw>4095 || state_.external) return; // On external power PC4 does not read the battery.
        average(raw);
        const uint32_t scaled=uint32_t(raw)*5000;
        const unsigned bin=scaled<=3400*4166U ? 5 : scaled<3500*4166U ? 0 :
            scaled<3600*4166U ? 1 : scaled<3800*4166U ? 2 : scaled<4000*4166U ? 3 : 4;
        ++histogram_[bin];
        if (++samples_<20) return;
        unsigned winner=0;
        for (unsigned i=1;i<6;++i) if (histogram_[i]>histogram_[winner]) winner=i;
        if (histogram_[winner]>=12) {
            state_.bars=winner==5 ? 1 : winner+1;
            state_.low=winner==5; state_.valid=true;
        }
        resetSamples();
    }
    static uint8_t percent(uint32_t millivolts) {
        static constexpr struct { uint16_t mv; uint8_t pct; } curve[]={
            {3400,0},{3500,8},{3600,20},{3700,40},{3800,58},{3900,72},{4000,84},{4100,93},{4200,100}};
        if (millivolts<=curve[0].mv) return 0;
        for (unsigned i=1;i<sizeof curve/sizeof curve[0];++i)
            if (millivolts<curve[i].mv)
                return uint8_t(curve[i-1].pct+(millivolts-curve[i-1].mv)*(curve[i].pct-curve[i-1].pct)/(curve[i].mv-curve[i-1].mv));
        return 100;
    }
private:
    void average(uint16_t raw) {
        sum_+=raw;
        if (++averaged_<20) return;
        const uint32_t millivolts=sum_*5000/4166/20; // 20 samples: 2 s at the 100 ms poll.
        state_.millivolts=uint16_t(millivolts); state_.percent=percent(millivolts); state_.measured=true;
        sum_=0; averaged_=0;
    }
    void resetSamples() { samples_=0; for (auto &count:histogram_) count=0; }
    BatteryState state_;
    uint8_t histogram_[6]{},samples_=0,candidate_=0,stable_=0,averaged_=0;
    uint32_t sum_=0;
};

class LowBatteryPolicy {
public:
    struct State { bool warning=false,powerOff=false; };
    State poll(const BatteryState &battery,uint32_t now) {
        if (!battery.valid || !battery.low || battery.external) { active_=false; return {}; }
        if (!active_) { active_=true; began_=retryAt_=now; warned_=offSent_=false; }
        State result;
        if (!warned_ && uint32_t(now-began_)>=40000) { result.warning=true; warned_=true; }
        if (uint32_t(now-began_)>=60000 && (!offSent_ || uint32_t(now-retryAt_)>=5000)) {
            result.powerOff=true; offSent_=true; retryAt_=now;
        }
        return result;
    }
private:
    uint32_t began_=0,retryAt_=0;
    bool active_=false,warned_=false,offSent_=false;
};
} // namespace openosc::power
