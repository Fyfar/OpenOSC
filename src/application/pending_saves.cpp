#include "openosc/application/pending_saves.hpp"
#include "openosc/settings/theme_store.hpp"

namespace openosc {
namespace {
bool due(uint32_t now, uint32_t at) { return int32_t(now - at) >= 0; }
} // namespace

uint32_t PendingSaves::service(uint32_t now, const ShutdownSequence &shutdown, bool handoffPending, bool hostActive, bool deferred,
                               Store &store) {
    if (shutdown.active() || handoffPending || hostActive || deferred) return 0;
    uint32_t outcome = 0;
    if (theme_ && due(now, themeAt_)) {
        if (store.saveTheme()) theme_ = false;
        else { themeAt_ = now + RetryMs; outcome |= ThemeFailed; }
    }
    if (settings_ && due(now, settingsAt_)) {
        outcome |= SettingsAttempted;
        if (store.savePreferences()) settings_ = false;
        else { settingsAt_ = now + RetryMs; outcome |= SettingsFailed; }
    }
    return outcome;
}
bool PendingSaves::flushTheme(Store &store) {
    if (theme_ && store.saveTheme()) theme_ = false;
    return !theme_;
}
bool PendingSaves::flushSettings(Store &store) {
    if (settings_ && store.savePreferences()) settings_ = false;
    return !settings_;
}
const char *ThemeNotice::text(const ThemeStore &themes) {
    if (themes.repaired()) return themes.restored() ? "Bad theme archived; previous copy restored" : "Bad theme archived; defaults restored";
    return themes.restored() ? "Theme restored from previous copy" : nullptr;
}
} // namespace openosc
