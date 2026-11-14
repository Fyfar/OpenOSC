#pragma once
#include "openosc/calibration/pwm_profile.hpp"
#include "openosc/calibration/calibration_file.hpp"
#include "openosc/storage/layout.hpp"
#include <cstddef>

namespace openosc::calibration {
// scope_pwm.json: schema 1, the factory CRC it belongs to, enabled, the node positions and V/div
// it was made for, and compare[channel][range][node].
class PwmJson {
public:
    using Profile = PwmProfile;
    static constexpr const char *Path = storage::Layout::PwmFile, *Previous = storage::Layout::PwmPrevious;
    static constexpr size_t MaxBytes = 4096;
    static constexpr CalibrationFileText Text{"Invalid hardware zero", "Cannot read hardware zero JSON",
        "Cannot save hardware zero JSON", "Cannot preserve invalid hardware zero JSON", "Hardware zero loaded; file restore failed"};
    static PwmProfile empty(const CalibrationData &factory) { PwmProfile profile; profile.factoryCrc = factory.crc(); return profile; }
    static bool parse(const char *text, size_t bytes, const CalibrationData &factory, PwmProfile &output);
    static size_t encode(const PwmProfile &values, const CalibrationData &factory, char *output, size_t capacity);
};
} // namespace openosc::calibration
