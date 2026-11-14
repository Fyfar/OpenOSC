#include "openosc/settings/theme_json.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/settings/theme_store.hpp"
#include "openosc/settings/theme_document.hpp"
#include "openosc/storage/json_files.hpp"

namespace openosc {
bool ThemeStore::savePlot(const plot::Options &options) {
    if (!options.valid()) {
        error_ = "INVALID DISPLAY OPTIONS";
        return false;
    }
    if (!fs_.mounted()) {
        error_ = "STORAGE NOT AVAILABLE";
        return false;
    }
    auto candidate = theme_;
    candidate.plot = options;
    char *buffer = ThemeScratch::input();
    const auto bytes = ThemeJson::encode(candidate, buffer, ThemeScratch::InputCapacity);
    if (!bytes || storage::ensureDirectory(storage::Layout::Ui) != FR_OK || !JsonFiles::write(storage::Layout::ThemeFile, buffer, bytes)) {
        error_ = "CANNOT SAVE DISPLAY JSON";
        return false;
    }
    theme_ = candidate;
    error_ = nullptr;
    return true;
}
} // namespace openosc
