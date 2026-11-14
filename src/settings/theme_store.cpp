#include "openosc/settings/theme_store.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/settings/theme_json.hpp"
#include "openosc/settings/theme_document.hpp"
#include "openosc/storage/json_files.hpp"
#include <cstring>

namespace openosc {
bool ThemeStore::reload() {
    error_ = nullptr; repaired_ = false;
    if (!fs_.mounted()) { error_ = "STORAGE NOT AVAILABLE"; return false; }
    char *buffer = ThemeScratch::input();
    size_t bytes = 0;
    const auto read = JsonFiles::read(storage::Layout::ThemeFile, buffer, ThemeScratch::InputCapacity, bytes);
    Theme candidate;
    const bool parsed = read == JsonFiles::ReadResult::Complete && ThemeParser::parse(buffer, bytes, candidate);
    const auto assets = parsed ? resources(candidate) : ResourceState::Invalid;
    if (parsed && assets == ResourceState::Valid) {
        theme_ = candidate;
        return true;
    }
    if (read == JsonFiles::ReadResult::StorageError) { error_ = "CANNOT READ THEME JSON"; return false; }
    if (assets == ResourceState::StorageError) { error_ = "CANNOT READ THEME RESOURCES"; return false; }
    if (read != JsonFiles::ReadResult::Missing) {
        if (!JsonFiles::archiveBad(storage::Layout::ThemeFile)) { error_ = "CANNOT PRESERVE INVALID THEME"; return false; }
        repaired_ = true;
    }
    candidate = Theme::defaults();
    const size_t length = ThemeJson::encode(candidate, buffer, ThemeScratch::InputCapacity);
    if (!length || storage::ensureDirectory(storage::Layout::Ui) != FR_OK || !JsonFiles::write(storage::Layout::ThemeFile, buffer, length)) {
        error_ = "CANNOT CREATE DEFAULT THEME JSON"; return false;
    }
    theme_ = candidate;
    return true;
}
ThemeStore::ResourceState ThemeStore::resources(const Theme& theme) {
    for (const auto& screen : theme.elements) {
        const auto& icon = screen[unsigned(ElementId::Icon)];
        if (!icon.visible) continue;
        if (!icon.icon[0]) return ResourceState::Invalid;
        const size_t length = std::strlen(icon.icon);
        if (length < 4 || std::strcmp(icon.icon + length - 4, ".bmp")) return ResourceState::Invalid;
        FIL file{};
        const auto opened = f_open(&file, icon.icon, FA_READ);
        if (opened == FR_NO_FILE || opened == FR_NO_PATH) return ResourceState::Invalid;
        if (opened != FR_OK) return ResourceState::StorageError;
        uint8_t header[54]; UINT count = 0;
        const bool sizeValid = f_size(&file) >= sizeof header && f_size(&file) <= 32768;
        const auto read = sizeValid ? f_read(&file, header, sizeof header, &count) : FR_OK;
        const FRESULT closed = f_close(&file);
        if (read != FR_OK || closed != FR_OK) return ResourceState::StorageError;
        if (!sizeValid || count != sizeof header || header[0] != 'B' || header[1] != 'M') return ResourceState::Invalid;
        int32_t width, height;
        std::memcpy(&width, header + 18, 4); std::memcpy(&height, header + 22, 4);
        // LVGL's BMP stream decoder expects bottom-up images.
        if (width < 1 || width > 64 || height < 1 || height > 64
            || (header[28] != 16 && header[28] != 24 && header[28] != 32)) return ResourceState::Invalid;
    }
    return ResourceState::Valid;
}
}
