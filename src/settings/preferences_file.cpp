#include "openosc/settings/preferences_file.hpp"
#include "openosc/settings/preferences_json.hpp"
#include "openosc/settings/json_workspace.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/json_files.hpp"

namespace openosc {
namespace {
// The file's text lies in the JSON workspace, as its document does (PreferencesJson): on the stack
// the two made a save from the main loop overflow it.
constexpr size_t TextBytes = PreferencesJson::MaxBytes + 1;
static_assert(TextBytes <= JsonWorkspace::TextBytes, "The settings text exceeds the JSON workspace");
}
bool PreferencesFile::readValid(const char* path, Preferences& output, bool& storageError) const {
    char *json = JsonWorkspace::text(); size_t bytes = 0; // SRAM4, not the stack (TextBytes).
    const auto read = JsonFiles::read(path, json, TextBytes, bytes);
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
    char *json = JsonWorkspace::text();
    const size_t bytes = PreferencesJson::encode(values, json, TextBytes);
    if (!bytes || !JsonFiles::write(Path, json, bytes)) { error_ = "CANNOT SAVE SETTINGS JSON"; return false; }
    values_ = values;
    return true;
}
}
