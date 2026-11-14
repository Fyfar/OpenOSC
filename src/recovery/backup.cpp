#include "openosc/recovery/backup.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/update/crc32.hpp"
#include "openosc/update/layout.hpp"
#include <cstdio>
#include <cstring>

namespace openosc {
namespace {
constexpr const char* Temporary = storage::Layout::FactoryBackupTemporary;
constexpr const char* Metadata = storage::Layout::FactoryBackupMetadata;
}
bool FactoryBackup::fail(const char* message) { error_ = message; return false; }
bool FactoryBackup::exists() const { FILINFO info{}; return f_stat(Path, &info) == FR_OK; }
bool FactoryBackup::copy() {
    FIL file{};
    if (f_open(&file, Temporary, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return fail("CANNOT CREATE BACKUP FILE");
    Crc32 crc;
    bool success = true;
    for (uint32_t offset = 0; success && offset < layout::FactoryBytes; offset += sizeof buffer_) {
        const auto* source = reinterpret_cast<const void*>(layout::FactoryBase + offset);
        UINT count = 0;
        crc.update(source, sizeof buffer_);
        success = f_write(&file, source, sizeof buffer_, &count) == FR_OK && count == sizeof buffer_;
        screen_.progress("COPYING FACTORY BLOCK", offset + count, layout::FactoryBytes);
    }
    if (success) success = f_sync(&file) == FR_OK;
    if (f_close(&file) != FR_OK) success = false;
    if (!success) return fail("BACKUP WRITE FAILED");
    crc_ = crc.value();
    return true;
}
bool FactoryBackup::verify() {
    FIL file{};
    if (f_open(&file, Temporary, FA_READ) != FR_OK) return fail("CANNOT VERIFY BACKUP FILE");
    bool success = f_size(&file) == layout::FactoryBytes;
    Crc32 crc;
    for (uint32_t offset = 0; success && offset < layout::FactoryBytes; offset += sizeof buffer_) {
        UINT count = 0;
        success = f_read(&file, buffer_, sizeof buffer_, &count) == FR_OK && count == sizeof buffer_;
        if (success) success = !std::memcmp(buffer_, reinterpret_cast<const void*>(layout::FactoryBase + offset), count);
        if (success) crc.update(buffer_, count);
        screen_.progress("VERIFYING FACTORY BACKUP", offset + count, layout::FactoryBytes);
    }
    if (f_close(&file) != FR_OK) success = false;
    return (success && crc.value() == crc_) || fail("BACKUP READBACK FAILED");
}
bool FactoryBackup::metadata() {
    char text[128];
    int length = std::snprintf(text, sizeof text,
        "OpenOSC factory block backup\r\nADDRESS=0x%08lX\r\nSIZE=%lu\r\nCRC32=%08lX\r\n",
        static_cast<unsigned long>(layout::FactoryBase), static_cast<unsigned long>(layout::FactoryBytes),
        static_cast<unsigned long>(crc_));
    if (length <= 0 || size_t(length) >= sizeof text) return fail("BACKUP METADATA SIZE ERROR");
    FIL file{};
    if (f_open(&file, Metadata, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return fail("CANNOT CREATE BACKUP METADATA");
    UINT count = 0;
    bool success = f_write(&file, text, length, &count) == FR_OK && count == unsigned(length);
    if (success) success = f_sync(&file) == FR_OK;
    if (f_close(&file) != FR_OK) success = false;
    return success || fail("BACKUP METADATA WRITE FAILED");
}
bool FactoryBackup::create(bool overwrite) {
    if (!fs_.mounted()) return fail("FAT VOLUME IS NOT AVAILABLE");
    if (exists() && !overwrite) return fail("BACKUP ALREADY EXISTS");
    if (!fs_.refreshFree() || fs_.freeBytes() < layout::FactoryBytes + 8192)
        return fail("NOT ENOUGH SPACE FOR BACKUP");
    if (!copy() || !verify()) return false;
    if (exists() && f_unlink(Path) != FR_OK) return fail("CANNOT REPLACE BACKUP FILE");
    if (f_rename(Temporary, Path) != FR_OK) return fail("CANNOT COMMIT BACKUP FILE");
    if (!metadata()) return false;
    error_ = nullptr;
    fs_.refreshFree();
    return true;
}
}
