#include "openosc/settings/preferences_file.hpp"
#include "openosc/settings/preferences_json.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/json_files.hpp"

namespace openosc {
bool PreferencesFile::readValid(const char* path, Preferences& output, bool& storageError) const {
    char json[PreferencesJson::MaxBytes + 1]; size_t bytes = 0;
    const auto read = JsonFiles::read(path, json, sizeof json, bytes);
    storageError = read == JsonFiles::ReadResult::StorageError;
    return read == JsonFiles::ReadResult::Complete && PreferencesJson::parse(json, bytes, output);
}
bool PreferencesFile::load() {
    values_ = Preferences{}; error_ = nullptr; repaired_ = restored_ = false;
    if (!fs_.mounted()) { error_ = "SETTINGS DISK IS NOT AVAILABLE"; return false; }
    bool storageError = false;
    Preferences candidate;
    if (readValid(Path, candidate, storageError)) { values_ = candidate; return true; }
    if (storageError) { error_ = "CANNOT READ SETTINGS JSON"; return false; }
    FILINFO info{};
    if (f_stat(Path, &info) == FR_OK) { // Present but invalid: kept aside, never overwritten.
        if (!JsonFiles::archiveBad(Path)) { error_ = "CANNOT PRESERVE INVALID SETTINGS"; return false; }
        repaired_ = true;
    }
    char previous[JsonFiles::PathBytes];
    if (JsonFiles::previousPath(Path, previous, sizeof previous) && readValid(previous, candidate, storageError)) {
        values_ = candidate; restored_ = true;
    } else if (storageError) { error_ = "CANNOT READ SETTINGS JSON"; return false; }
    return save(values_);
}
bool PreferencesFile::save(const Preferences& values) {
    error_ = nullptr;
    if (!fs_.mounted() || storage::ensureDirectory(storage::Layout::Settings) != FR_OK) { error_ = "SETTINGS DISK IS NOT AVAILABLE"; return false; }
    char json[PreferencesJson::MaxBytes + 1];
    const size_t bytes = PreferencesJson::encode(values, json, sizeof json);
    if (!bytes || !JsonFiles::write(Path, json, bytes)) { error_ = "CANNOT SAVE SETTINGS JSON"; return false; }
    values_ = values;
    return true;
}
}
