#pragma once
#include "openosc/calibration/calibration_file.hpp"
#include "openosc/calibration/pwm_json.hpp"

namespace openosc::calibration {
// The hardware-zero profile on the calibration disk (scope_pwm.json, as ZeroFile).
class PwmFile final : public CalibrationFile<PwmJson> {
public:
    using CalibrationFile::CalibrationFile;
};
} // namespace openosc::calibration
