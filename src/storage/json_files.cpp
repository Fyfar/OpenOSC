#include "openosc/storage/json_files.hpp"
#include "openosc/update/crc32.hpp"
#include "ff.h"
#include <cstdio>
#include <cstring>

namespace openosc {
JsonFiles::ReadResult JsonFiles::read(const char* path, char* output, size_t capacity, size_t& bytes) {
    bytes = 0;
    FIL file{};
    const FRESULT opened = f_open(&file, path, FA_READ);
    if (opened == FR_NO_FILE || opened == FR_NO_PATH) return ReadResult::Missing;
    if (opened != FR_OK) return ReadResult::StorageError;
    const size_t size = f_size(&file);
    const bool badSize = !size || size >= capacity;
    UINT count = 0;
    const FRESULT read = badSize ? FR_OK : f_read(&file, output, size, &count);
    const FRESULT closed = f_close(&file);
    if (read != FR_OK || closed != FR_OK || (!badSize && count != size)) return ReadResult::StorageError;
    if (badSize) return ReadResult::BadSize;
    bytes = count; output[count] = 0;
    return ReadResult::Complete;
}
bool JsonFiles::archiveBad(const char* path) {
    const char* dot = std::strrchr(path, '.');
    if (!dot || dot - path > 80) return false;
    char archived[PathBytes];
    for (unsigned n = 1; n <= 9999; ++n) {
        std::snprintf(archived, sizeof archived, "%.*s.bad.%04u%s", int(dot - path), path, n, dot);
        FILINFO info{};
        const FRESULT exists = f_stat(archived, &info);
        if (exists == FR_OK) continue;
        if (exists != FR_NO_FILE) return false;
        return f_rename(path, archived) == FR_OK;
    }
    return false;
}
bool JsonFiles::previousPath(const char* path, char* output, size_t size) {
    const char* dot = path ? std::strrchr(path, '.') : nullptr;
    if (!dot || !output) return false;
    const int length = std::snprintf(output, size, "%.*s.previous%s", int(dot - path), path, dot);
    return length > 0 && size_t(length) < size;
}
namespace {
bool verify(const char* path, const char* text, size_t bytes) {
    FIL file{};
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    bool success = f_size(&file) == bytes;
    Crc32 crc;
    uint8_t buffer[256];
    for (size_t offset = 0; success && offset < bytes;) {
        const size_t chunk = bytes - offset > sizeof buffer ? sizeof buffer : bytes - offset;
        UINT count = 0;
        success = f_read(&file, buffer, chunk, &count) == FR_OK && count == chunk;
        if (success) { crc.update(buffer, chunk); offset += chunk; }
    }
    const FRESULT closed = f_close(&file);
    return success && closed == FR_OK && crc.value() == Crc32::compute(text, bytes);
}
}
bool JsonFiles::write(const char* path, const char* text, size_t bytes) {
    if (!path || !text || std::strlen(path) > 90 || !bytes) return false;
    char temporary[PathBytes], previous[PathBytes];
    std::snprintf(temporary, sizeof temporary, "%s.tmp", path);
    if (!previousPath(path, previous, sizeof previous)) return false;
    FIL file{};
    if (f_open(&file, temporary, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    UINT written = 0;
    const FRESULT write = f_write(&file, text, bytes, &written);
    const FRESULT synced = f_sync(&file), closed = f_close(&file);
    if (write != FR_OK || written != bytes || synced != FR_OK || closed != FR_OK || !verify(temporary, text, bytes)) return false;
    FILINFO info{};
    const FRESULT exists = f_stat(path, &info);
    bool moved = false;
    if (exists == FR_OK) {
        const FRESULT removed = f_unlink(previous);
        if (removed != FR_OK && removed != FR_NO_FILE) return false;
        if (f_rename(path, previous) != FR_OK) return false;
        moved = true;
    } else if (exists != FR_NO_FILE) return false;
    if (f_rename(temporary, path) == FR_OK) return true;
    if (moved) f_rename(previous, path);
    return false;
}
}
