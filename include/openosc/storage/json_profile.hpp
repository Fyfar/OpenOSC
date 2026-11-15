#pragma once
#include "openosc/storage/json_files.hpp"

namespace openosc {
// How a JSON profile was read through JsonFiles' transactional names: the current file, else the
// previous one (left alone by JsonFiles::write when power is lost between its two renames). Both are
// read and judged before anything changes: a storage error in either stops with nothing archived or
// written; only then is each invalid file archived (also an invalid current before a valid previous).
// Shared by the calibration files and the theme.
class JsonProfile {
public:
    enum class Load { Current, Previous, Missing, Repaired, StorageError, ArchiveError };
    // What parse made of a complete file: a valid profile (kept by parse), invalid content, or a
    // storage error in what the content refers to (the theme's icons).
    enum class Verdict { Valid, Invalid, StorageError };
    // parse(json, bytes) -> bool (valid) or Verdict; archived: an invalid file was set aside.
    template <class Parse> static Load load(const char *path, const char *previous, char *json, size_t capacity, bool &archived, Parse parse) {
        const char *paths[] = {path, previous};
        bool invalid[2] = {false, false};
        archived = false;
        unsigned valid = 2; // None.
        for (unsigned i = 0; i < 2 && valid == 2; ++i) {
            size_t bytes = 0;
            const auto read = JsonFiles::read(paths[i], json, capacity, bytes);
            if (read == JsonFiles::ReadResult::StorageError) return Load::StorageError;
            if (read == JsonFiles::ReadResult::Missing) continue;
            const auto verdict = read == JsonFiles::ReadResult::Complete ? judge(parse(json, bytes)) : Verdict::Invalid;
            if (verdict == Verdict::StorageError) return Load::StorageError;
            if (verdict == Verdict::Valid) valid = i; else invalid[i] = true;
        }
        for (unsigned i = 0; i < 2; ++i) {
            if (!invalid[i]) continue;
            if (!JsonFiles::archiveBad(paths[i])) return Load::ArchiveError;
            archived = true;
        }
        if (valid < 2) return valid ? Load::Previous : Load::Current;
        return archived ? Load::Repaired : Load::Missing;
    }
private:
    static Verdict judge(bool valid) { return valid ? Verdict::Valid : Verdict::Invalid; }
    static Verdict judge(Verdict verdict) { return verdict; }
};
} // namespace openosc
