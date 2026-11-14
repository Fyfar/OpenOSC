#include "openosc/calibration/pwm_solver.hpp"
#include <algorithm>
#include <cmath>

namespace openosc::calibration {
void PwmSolver::begin(uint16_t start,double codesPerCount) {
    *this={};
    compare_=best_=std::min<uint16_t>(start,9999);
    slope_=codesPerCount;
}
PwmSolver::Step PwmSolver::converge(uint16_t compare) {
    converged_=true; compare_=compare;
    return Step::Converged;
}
PwmSolver::Step PwmSolver::update(double error) {
    if (converged_) return Step::Converged;
    if (!std::isfinite(error) || !std::isfinite(slope_) || slope_==0) return Step::Failed;
    ++iterations_;
    if (iterations_==1 || std::abs(error)<std::abs(bestError_)) { best_=compare_; bestError_=error; }
    if (std::abs(error)<=ToleranceCodes) return converge(compare_);
    // The bracket: a measured compare on each side of the answer.
    if (error<0) { low_=compare_; lowError_=error; haveLow_=true; }
    else { high_=compare_; highError_=error; haveHigh_=true; }
    if (haveLow_ && haveHigh_ && std::abs(int(high_)-int(low_))<=1) {
        // Neighbours on either side: one count apart is what their errors may differ by, at most
        // four factory slopes; a larger jump is no resolution of a working PWM.
        if (highError_-lowError_>4*std::abs(slope_)+2*ToleranceCodes) return Step::Failed;
        return converge(std::abs(lowError_)<=std::abs(highError_) ? low_ : high_);
    }
    double slope=slope_,damping=.5; // Half a factory step until a secant says what the PWM does.
    if (havePrevious_ && compare_!=previous_) {
        const double moved=double(compare_)-previous_,change=error-previousError_,secant=change/moved;
        // A step the factory slope makes worth a code or more, that moved the error by less than
        // SignificantCodes: the PWM does not respond (or the input is held elsewhere).
        if (std::abs(slope_*moved)>=8*SignificantCodes && std::abs(change)<SignificantCodes && ++unresponsive_>=2) return Step::Failed;
        const bool sameSign=(secant>0)==(slope_>0);
        if (!sameSign && std::abs(change)>=SignificantCodes && ++wrongSign_>=2) return Step::Failed;
        if (sameSign && std::abs(secant)>=std::abs(slope_)/4 && std::abs(secant)<=std::abs(slope_)*4) { slope=secant; damping=1; }
    }
    if (iterations_>=MaximumIterations)
        return std::abs(bestError_)<=2*ToleranceCodes ? converge(best_) : Step::Failed;
    const double limit=std::ceil(ReachCodes/std::abs(slope_));
    double next=compare_-std::clamp(damping*error/slope,-limit,limit);
    if (haveLow_ && haveHigh_) {
        const double a=std::min(low_,high_),b=std::max(low_,high_);
        if (!(next>a && next<b)) next=(a+b)/2;
    }
    long rounded=std::lround(next);
    if (rounded==long(compare_)) {
        // Under half a count to go: the resolution, if the rest is what half a count explains;
        // else one count towards the answer.
        if (std::abs(error)<=std::abs(slope)/2+ToleranceCodes) return converge(best_);
        rounded=long(compare_)+(error/slope>0 ? -1 : 1);
    }
    // The answer lies past the compare's ends: nothing to try (a PWM or table far off, a DC input).
    if (rounded<0 || rounded>9999) {
        if (compare_==(rounded<0 ? 0 : 9999)) return Step::Failed;
        rounded=std::clamp(rounded,0L,9999L);
    }
    previous_=compare_; previousError_=error; havePrevious_=true;
    compare_=uint16_t(rounded);
    return Step::Continue;
}
} // namespace openosc::calibration
