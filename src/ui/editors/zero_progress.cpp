#include "openosc/ui/editors/zero_progress.hpp"
#include "openosc/ui/scope/format.hpp"
#include "openosc/calibration/zero_profile.hpp"
#include <cstdio>

namespace openosc::ui {
namespace {
using Phase=calibration::ZeroOperation::Phase;
const char *stateName(const calibration::ZeroProcedure &process,Phase phase) {
    return phase==Phase::Applying ? "Applying" : phase==Phase::Restoring ? "Restoring" : phase==Phase::Resuming ? "New capture" :
        process.state()==calibration::ZeroProcedure::State::Settling ? "Settling" : "Sampling";
}
bool counting(Phase phase) { return phase==Phase::Applying || phase==Phase::Collecting; }
}
void ZeroProgress::name(unsigned row,char *output,size_t capacity) const {
    if (row==0) std::snprintf(output,capacity,"Range");
    else if (row==1) std::snprintf(output,capacity,"Progress");
    else if (row==2) std::snprintf(output,capacity,"%s",counting(phase_) ? stateName(process_,phase_) : "State");
    else std::snprintf(output,capacity,"%s",phase_==Phase::Restoring || phase_==Phase::Resuming ? "Resume RUN" : "Cancel");
}
void ZeroProgress::value(unsigned row,char *output,size_t capacity) const {
    if (row==0) {
        const unsigned index=process_.range()<calibration::ZeroProfile::Ranges ? process_.range() : calibration::ZeroProfile::Ranges-1;
        plot::Format::quantity(scope::VoltageRanges::millivolts(index)/1000.0,"V/div",output,capacity);
    } else if (row==1) std::snprintf(output,capacity,"%u%%",process_.percent());
    else if (row==2) {
        if (!counting(phase_)) std::snprintf(output,capacity,"%s",stateName(process_,phase_));
        else {
            process_.detail(output,capacity); // The hardware zero's node and iteration.
            if (!*output) std::snprintf(output,capacity,"%u/%u",process_.point()+1,calibration::ZeroProfile::Points);
        }
    } else if (capacity) output[0]=0;
}
} // namespace openosc::ui
