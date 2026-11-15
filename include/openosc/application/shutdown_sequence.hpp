#pragma once
#include "openosc/platform/shutdown.hpp"
#include <cstdint>

namespace openosc {
// Power off, restart and the Recovery handoff as one bounded sequence (docs/POWER.md). Pure: the
// controller supplies the steps; every wait has a deadline and every save is tried once.
//
// 1. Screenshot: an unfinished one may complete for ScreenshotMs, then it is cancelled (its temporary
//    file removed) so that nothing writes it after the unmount.
// 2. Save the pending theme/plot and settings once (the normal atomic writer, unchanged).
// 3. Stop the instruments; recorders may finish for FinishMs, then they are cancelled.
// 4. Unmount the volume and restore the NOR's legacy addressing.
// 5. The action, after NoticeMs on screen if anything above failed.
//
// Policy on failures:
// - PowerOff (key hold, low battery, auto off) always ends powered off: a failed save or storage step
//   is shown, never a reason to stay on.
// - Recovery always hands off: it is where a broken volume is repaired.
// - Restart is refused if a save or the storage steps failed (a reset would silently drop the
//   changes, or let the factory boot read a volume that was not finished); the instrument resumes and
//   says why. Power off and Recovery remain.
class ShutdownSequence {
public:
    static constexpr uint32_t ScreenshotMs = 3000, FinishMs = 10000, NoticeMs = 1500;
    enum Problem : uint32_t {
        ScreenshotUnfinished = 1, ThemeNotSaved = 2, SettingsNotSaved = 4, FilesCancelled = 8,
        StorageNotFinished = 16, AddressingNotRestored = 32,
    };
    class Steps {
    public:
        virtual ~Steps() = default;
        virtual void prepare() = 0;              // Cancel background reads (image viewer, clearing).
        virtual bool screenshotBusy() = 0;
        virtual void cancelScreenshot() = 0;     // Close and remove an unfinished one: nothing writes it later.
        virtual bool saveTheme() = 0;            // True when nothing was pending or it is saved.
        virtual bool savePreferences() = 0;
        virtual void stopInstruments() = 0;      // USB, capture, DMM, GEN; recorders finish their files.
        virtual bool instrumentsStopped() = 0;
        virtual void cancelRecorders() = 0;
        virtual bool unmount() = 0;
        virtual bool restoreAddressing() = 0;
        virtual void notice(const char *text) = 0;
        virtual void finish(ShutdownAction action) = 0; // Powers off or resets: does not return on the device.
        virtual bool resume() = 0;               // A refused restart after the stop: everything back.
    };
    enum class Phase : uint8_t { Idle, Screenshot, Finishing, Holding };

    // False if a sequence already runs or `action` is None.
    bool begin(ShutdownAction action, uint32_t now, Steps &steps);
    void service(uint32_t now, Steps &steps);
    bool active() const { return phase_ != Phase::Idle; }
    ShutdownAction action() const { return active() ? action_ : ShutdownAction::None; }
    Phase phase() const { return phase_; }
    uint32_t problems() const { return problems_; }
    // The one line the instrument shows for these problems (nullptr: none).
    static const char *summary(ShutdownAction action, uint32_t problems);

private:
    void save(uint32_t now, Steps &steps);
    void store(uint32_t now, Steps &steps);
    void refuse(Steps &steps, bool stopped);
    ShutdownAction action_ = ShutdownAction::None;
    Phase phase_ = Phase::Idle;
    uint32_t deadline_ = 0, problems_ = 0;
};
} // namespace openosc
