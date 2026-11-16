#include "openosc/settings/preferences_file.hpp"
#include "openosc/settings/preferences_json.hpp"
#include "openosc/settings/json_workspace.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/json_files.hpp"

namespace openosc {
bool PreferencesFile::readValid(const char* path, Preferences& output, bool& storageError) const {
    static_assert(PreferencesJson::MaxBytes + 1 <= JsonWorkspace::TextCapacity);
    JsonWorkspace::Text json; size_t bytes = 0; // Off the stack (JsonWorkspace).
    if (!json || JsonWorkspace::documentHeld()) { storageError = true; return false; } // Busy, not invalid.
    const auto read = JsonFiles::read(path, json.data(), PreferencesJson::MaxBytes + 1, bytes);
    storageError = read == JsonFiles::ReadResult::StorageError;
    return read == JsonFiles::ReadResult::Complete && PreferencesJson::parse(json.data(), bytes, output);
}
bool PreferencesFile::load() {
    // A busy workspace is unreadable now: nothing changes, nothing is archived or written.
    if (!JsonWorkspace::idle()) { error_ = "CANNOT READ SETTINGS JSON"; repaired_ = restored_ = false; return false; }
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
    JsonWorkspace::Text json; // Off the stack (JsonWorkspace).
    const size_t bytes = json ? PreferencesJson::encode(values, json.data(), PreferencesJson::MaxBytes + 1) : 0;
    if (!bytes || !JsonFiles::write(Path, json.data(), bytes)) { error_ = "CANNOT SAVE SETTINGS JSON"; return false; }
    values_ = values;
    return true;
}
}
