#include "openosc/settings/theme_store.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/settings/theme_json.hpp"
#include "openosc/settings/theme_document.hpp"
#include "openosc/storage/json_files.hpp"
#include "openosc/storage/json_profile.hpp"
#include <cstring>

namespace openosc {
// theme.json, else theme.previous.json (a power loss between JsonFiles::write's two renames), as
// the calibration files (JsonProfile). Invalid files are archived; a read error in the JSON or an
// icon stops before anything is archived or written. A restored previous theme is written back as
// theme.json; with neither valid, the defaults are.
bool ThemeStore::reload() {
    error_ = nullptr; repaired_ = restored_ = false;
    if (!fs_.mounted()) { error_ = "STORAGE NOT AVAILABLE"; return false; }
    char previous[JsonFiles::PathBytes];
    if (!JsonFiles::previousPath(storage::Layout::ThemeFile, previous, sizeof previous)) { error_ = "CANNOT READ THEME JSON"; return false; }
    Theme candidate;
    bool resourceError = false;
    using Load = JsonProfile::Load;
    using Verdict = JsonProfile::Verdict;
    const auto load = JsonProfile::load(storage::Layout::ThemeFile, previous, ThemeScratch::input(), ThemeScratch::InputCapacity, repaired_,
        [&](const char *text, size_t bytes) {
            if (!ThemeParser::parse(text, bytes, candidate)) return Verdict::Invalid;
            const auto assets = resources(candidate);
            resourceError = assets == ResourceState::StorageError;
            return assets == ResourceState::Valid ? Verdict::Valid : resourceError ? Verdict::StorageError : Verdict::Invalid;
        });
    switch (load) {
    case Load::Current: theme_ = candidate; return true;
    case Load::Previous:
        theme_ = candidate; restored_ = true;
        if (!write(candidate)) error_ = "THEME RESTORED; CANNOT WRITE THEME JSON"; // In use even so.
        return true;
    case Load::StorageError: error_ = resourceError ? "CANNOT READ THEME RESOURCES" : "CANNOT READ THEME JSON"; return false;
    case Load::ArchiveError: error_ = "CANNOT PRESERVE INVALID THEME"; return false;
    case Load::Missing: case Load::Repaired: break;
    }
    candidate = Theme::defaults();
    if (!write(candidate)) { error_ = "CANNOT CREATE DEFAULT THEME JSON"; return false; }
    theme_ = candidate;
    return true;
}
bool ThemeStore::write(const Theme &theme) {
    char *buffer = ThemeScratch::input();
    const size_t length = ThemeJson::encode(theme, buffer, ThemeScratch::InputCapacity);
    return length && storage::ensureDirectory(storage::Layout::Ui) == FR_OK && JsonFiles::write(storage::Layout::ThemeFile, buffer, length);
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
