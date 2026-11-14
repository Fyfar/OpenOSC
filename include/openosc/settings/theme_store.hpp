#pragma once
#include "openosc/settings/theme.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc {
class ThemeStore {
public:
    explicit ThemeStore(Filesystem& fs) : fs_(fs) {}
    bool reload();
    bool savePlot(const plot::Options& options);
    void defaults() { theme_ = Theme::defaults(); }
    const Theme& theme() const { return theme_; }
    const char* error() const { return error_; }
    bool repaired() const { return repaired_; }
private:
    enum class ResourceState { Valid, Invalid, StorageError };
    ResourceState resources(const Theme& theme);
    Filesystem& fs_;
    Theme theme_ = Theme::defaults();
    const char* error_ = nullptr;
    bool repaired_ = false;
};
}
