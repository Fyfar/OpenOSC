#include "openosc/calibration/zero_selection.hpp"
#include "openosc/calibration/zero_file.hpp"
#include "openosc/calibration/pwm_file.hpp"
#include "openosc/calibration/scope_calibration.hpp"

namespace openosc::calibration {
ZeroSelection::Result ZeroSelection::useFactory(ZeroFile &software,PwmFile &hardware,ScopeCalibration &calibration) {
    if (!software.loaded() || !hardware.loaded()) return Result::Unread;
    // Only what is on is written: an unneeded write cannot fail.
    const bool softwareOn=software.values().enabled;
    if (softwareOn) {
        auto values=software.values(); values.enabled=false;
        if (!software.save(values)) return Result::Unchanged;
    }
    if (hardware.values().enabled) {
        auto values=hardware.values(); values.enabled=false;
        if (!hardware.save(values)) return softwareOn ? Result::HardwareKept : Result::Unchanged;
        calibration.setPwm(values); // A disabled profile is always accepted.
    }
    return Result::Factory;
}
} // namespace openosc::calibration
