#pragma once
#include "openosc/calibration/gain_profile.hpp"
#include "openosc/calibration/calibration_file.hpp"
#include <cstddef>

namespace openosc::calibration {
// scope_gain.json: schema 1, the factory CRC, enabled, the V/div it was made for and gain[channel][range].
class GainJson {
public:
    using Profile = GainProfile;
    static constexpr const char *Path = storage::Layout::GainFile, *Previous = storage::Layout::GainPrevious;
    static constexpr size_t MaxBytes = 1024;
    static constexpr CalibrationFileText Text{"Invalid gain correction", "Cannot read gain JSON", "Cannot save gain JSON",
                                              "Cannot preserve invalid gain JSON", "Gain loaded; file restore failed"};
    static GainProfile empty(const CalibrationData &factory) { GainProfile profile; profile.factoryCrc = factory.crc(); return profile; }
    static bool parse(const char *text, size_t bytes, const CalibrationData &factory, GainProfile &output);
    static size_t encode(const GainProfile &values, const CalibrationData &factory, char *output, size_t capacity);
};
class GainFile final : public CalibrationFile<GainJson>, public GainStore {
public:
    using CalibrationFile::CalibrationFile;
    bool available() const override { return CalibrationFile::available(); }
    bool loaded() const override { return CalibrationFile::loaded(); }
    const GainProfile &values() const override { return CalibrationFile::values(); }
    bool save(const GainProfile &values) override { return CalibrationFile::save(values); }
};
} // namespace openosc::calibration
