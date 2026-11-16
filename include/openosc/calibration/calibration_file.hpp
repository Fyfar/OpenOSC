#pragma once
#include "openosc/calibration/data.hpp"
#include "openosc/calibration/profile_store.hpp"
#include "openosc/settings/json_workspace.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/filesystem.hpp"
#include "openosc/storage/layout.hpp"

namespace openosc::calibration {
// What a calibration file's errors say.
struct CalibrationFileText {
    const char *invalid, *unreadable, *unsaved, *unarchived, *unrestored;
};
// A user calibration profile on the calibration disk bound to this factory record (its CRC), read
// and written through JsonFiles' transactional names (ProfileStore). The factory record is only
// read. Codec: Profile (with valid(factory)), Path, Previous, MaxBytes, Text, empty(factory),
// parse(text, bytes, factory, out) and encode(values, factory, out, capacity).
template <class Codec> class CalibrationFile {
public:
    using Profile = typename Codec::Profile;
    static constexpr const char *Path = Codec::Path, *Previous = Codec::Previous;
    CalibrationFile(Filesystem &fs, const CalibrationData &factory) : fs_(fs), factory_(factory) {}
    bool load() { loaded_ = read(); return loaded_; }
    bool save(const Profile &values) {
        error_ = nullptr;
        if (!values.valid(factory_)) { error_ = Codec::Text.invalid; return false; }
        if (!fs_.mounted() || storage::ensureDirectory(storage::Layout::Calibration) != FR_OK) { error_ = "Calibration disk unavailable"; return false; }
        char *json = JsonWorkspace::text(); // Not the stack; read() has parsed its text by now.
        const size_t bytes = Codec::encode(values, factory_, json, TextBytes);
        if (!bytes || !JsonFiles::write(Path, json, bytes)) { error_ = Codec::Text.unsaved; return false; }
        values_ = values; loaded_ = true;
        return true;
    }
    const Profile &values() const { return values_; }
    const char *error() const { return error_; }
    bool repaired() const { return repaired_; }
    // values() are what the disk holds: the last load or a save succeeded (never after a failed read).
    bool loaded() const { return loaded_; }
    bool available() const { return fs_.mounted(); }
private:
    static constexpr size_t TextBytes = Codec::MaxBytes + 1;
    static_assert(TextBytes <= JsonWorkspace::TextBytes, "The profile text exceeds the JSON workspace");
    bool read() {
        error_ = nullptr; repaired_ = false;
        const Profile empty = Codec::empty(factory_);
        if (values_.factoryCrc != factory_.crc()) values_ = empty;
        if (!fs_.mounted()) { error_ = "Calibration disk unavailable"; return false; }
        char *json = JsonWorkspace::text();
        Profile candidate = empty;
        using Load = ProfileStore::Load;
        switch (ProfileStore::load(Path, Previous, json, TextBytes, repaired_, [&](const char *text, size_t bytes) {
                    return Codec::parse(text, bytes, factory_, candidate); })) {
        case Load::Current: values_ = candidate; return true;
        case Load::Previous: // A power loss between the two renames: the verified previous profile.
            values_ = candidate;
            if (!save(candidate)) error_ = Codec::Text.unrestored;
            return true;
        case Load::StorageError: error_ = Codec::Text.unreadable; return false;
        case Load::ArchiveError: error_ = Codec::Text.unarchived; return false;
        case Load::Repaired: return save(empty);
        case Load::Missing: break;
        }
        values_ = empty; // No file: the factory's.
        return true;
    }
    Filesystem &fs_;
    const CalibrationData &factory_;
    Profile values_;
    const char *error_ = nullptr;
    bool repaired_ = false, loaded_ = false;
};
} // namespace openosc::calibration
