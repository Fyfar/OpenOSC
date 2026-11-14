#pragma once
#include "openosc/storage/json_files.hpp"

namespace openosc::calibration {
// How a calibration profile file was read (JsonFiles' transactional names): the current file, else
// the previous one (a power loss between the two renames), each invalid one archived on the way.
class ProfileStore {
public:
    enum class Load { Current, Previous, Missing, Repaired, StorageError, ArchiveError };
    // parse(json, bytes) -> true when the text is a valid profile (and keeps it); archived: an
    // invalid file was set aside on the way (also before a valid previous one).
    template <class Parse> static Load load(const char *path, const char *previous, char *json, size_t capacity, bool &archived, Parse parse) {
        const char *paths[] = {path, previous};
        bool &repaired = archived;
        repaired = false;
        for (unsigned i = 0; i < 2; ++i) {
            size_t bytes = 0;
            const auto read = JsonFiles::read(paths[i], json, capacity, bytes);
            if (read == JsonFiles::ReadResult::StorageError) return Load::StorageError;
            if (read == JsonFiles::ReadResult::Missing) continue;
            if (read == JsonFiles::ReadResult::Complete && parse(json, bytes)) return i ? Load::Previous : Load::Current;
            if (!JsonFiles::archiveBad(paths[i])) return Load::ArchiveError;
            repaired = true;
        }
        return repaired ? Load::Repaired : Load::Missing;
    }
};
} // namespace openosc::calibration
