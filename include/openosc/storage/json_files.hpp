#pragma once
#include <cstddef>

namespace openosc {
class JsonFiles {
public:
    enum class ReadResult { Complete, Missing, BadSize, StorageError };
    static ReadResult read(const char* path, char* output, size_t capacity, size_t& bytes);
    static bool archiveBad(const char* path);
    // Atomic: a verified temporary replaces `path`; the replaced file is kept as previousPath().
    static bool write(const char* path, const char* text, size_t bytes);
    static constexpr size_t PathBytes = 112; // Any path these functions derive (.tmp, .previous, .bad.NNNN).
    // "dir/name.previous.ext" for "dir/name.ext"; false if it does not fit or `path` has no extension.
    static bool previousPath(const char* path, char* output, size_t size);
};
}
