#pragma once
#include "openosc/application/shutdown_sequence.hpp"
#include <cstdint>

namespace openosc {
// The theme/plot and settings files waiting to be written (docs/POWER.md).
//
// - service(): the main loop's background save, DelayMs after a change, RetryMs after a failure.
// - It writes nothing while a USB host is active (STORE-02), and nothing while a shutdown or the
//   Recovery handoff runs. ShutdownSequence saves once itself (flush*), and after its unmount nothing
//   may touch the volume again.
// - `deferred`: the instrument asks for a moment without NOR writes (HOLD waiting for its record,
//   DataAccess::scopeHoldPending, at most 2 s). Nothing is written and no deadline moves; what is
//   due is written on the first pass after. flush* is not deferred.
class PendingSaves {
public:
    static constexpr uint32_t DelayMs = 1000, RetryMs = 5000;
    class Store {
    public:
        virtual ~Store() = default;
        virtual bool saveTheme() = 0;
        virtual bool savePreferences() = 0;
    };
    enum Outcome : uint32_t { ThemeFailed = 1, SettingsFailed = 2, SettingsAttempted = 4 };

    void settingsChanged(uint32_t now) { settings_ = true; settingsAt_ = now + DelayMs; }
    void plotChanged(uint32_t now) { theme_ = true; themeAt_ = now + DelayMs; }
    void dropTheme() { theme_ = false; }
    void dropSettings() { settings_ = false; }
    bool themePending() const { return theme_; }
    bool settingsPending() const { return settings_; }
    // Outcome bits of this pass; 0 when nothing was due or the volume is closed to background saves.
    uint32_t service(uint32_t now, const ShutdownSequence &shutdown, bool handoffPending, bool hostActive, bool deferred,
                     Store &store);
    // ShutdownSequence's one attempt each: true when nothing was pending or it is saved.
    bool flushTheme(Store &store);
    bool flushSettings(Store &store);

private:
    bool theme_ = false, settings_ = false;
    uint32_t themeAt_ = 0, settingsAt_ = 0;
};

class ThemeStore;
// What a theme load says after a reload (nullptr: a plain load).
struct ThemeNotice {
    static const char *text(const ThemeStore &themes);
};
} // namespace openosc
