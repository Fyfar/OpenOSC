#pragma once
#include "openosc/calibration/interleave_profile.hpp"
#include "openosc/calibration/calibration_file.hpp"
#include <cstddef>

namespace openosc::calibration {
// scope_interleave.json: schema 1, the factory CRC, enabled and offset_codes.
class InterleaveJson {
public:
    using Profile = InterleaveProfile;
    static constexpr const char *Path = storage::Layout::InterleaveFile, *Previous = storage::Layout::InterleavePrevious;
    static constexpr size_t MaxBytes = 256;
    static constexpr CalibrationFileText Text{"Invalid interleave offset", "Cannot read interleave JSON", "Cannot save interleave JSON",
                                              "Cannot preserve invalid interleave JSON", "Interleave loaded; file restore failed"};
    static InterleaveProfile empty(const CalibrationData &factory) { InterleaveProfile profile; profile.factoryCrc = factory.crc(); return profile; }
    static bool parse(const char *text, size_t bytes, const CalibrationData &factory, InterleaveProfile &output);
    static size_t encode(const InterleaveProfile &values, const CalibrationData &factory, char *output, size_t capacity);
};
class InterleaveFile final : public CalibrationFile<InterleaveJson> {
public:
    using CalibrationFile::CalibrationFile;
};
} // namespace openosc::calibration
