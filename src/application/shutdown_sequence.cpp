#include "openosc/application/shutdown_sequence.hpp"

namespace openosc {
namespace {
bool reached(uint32_t now, uint32_t deadline) { return int32_t(now - deadline) >= 0; }
constexpr uint32_t RestartBlocking = ShutdownSequence::ThemeNotSaved | ShutdownSequence::SettingsNotSaved |
                                     ShutdownSequence::StorageNotFinished | ShutdownSequence::AddressingNotRestored;
} // namespace

bool ShutdownSequence::begin(ShutdownAction action, uint32_t now, Steps &steps) {
    if (active() || action == ShutdownAction::None) return false;
    action_ = action; problems_ = 0;
    phase_ = Phase::Screenshot; deadline_ = now + ScreenshotMs;
    steps.prepare();
    service(now, steps);
    return true;
}
void ShutdownSequence::service(uint32_t now, Steps &steps) {
    switch (phase_) {
    case Phase::Idle: return;
    case Phase::Screenshot:
        if (steps.screenshotBusy()) {
            if (!reached(now, deadline_)) return;
            steps.cancelScreenshot();
            problems_ |= ScreenshotUnfinished;
        }
        save(now, steps);
        return;
    case Phase::Finishing:
        if (!steps.instrumentsStopped()) {
            if (!reached(now, deadline_)) return;
            steps.cancelRecorders();
            problems_ |= FilesCancelled;
        }
        store(now, steps);
        return;
    case Phase::Holding:
        if (!reached(now, deadline_)) return;
        phase_ = Phase::Idle;
        steps.finish(action_);
        return;
    }
}
// Each save once: a failure is recorded, not retried here.
void ShutdownSequence::save(uint32_t now, Steps &steps) {
    if (!steps.saveTheme()) problems_ |= ThemeNotSaved;
    if (!steps.savePreferences()) problems_ |= SettingsNotSaved;
    if (action_ == ShutdownAction::Restart && (problems_ & RestartBlocking)) { refuse(steps, false); return; }
    steps.stopInstruments();
    steps.notice("Finishing files...");
    phase_ = Phase::Finishing; deadline_ = now + FinishMs;
}
void ShutdownSequence::store(uint32_t now, Steps &steps) {
    if (!steps.unmount()) problems_ |= StorageNotFinished;
    if (!steps.restoreAddressing()) problems_ |= AddressingNotRestored;
    if (action_ == ShutdownAction::Restart && (problems_ & RestartBlocking)) { refuse(steps, true); return; }
    if (const char *text = summary(action_, problems_)) {
        steps.notice(text);
        phase_ = Phase::Holding; deadline_ = now + NoticeMs;
        return;
    }
    phase_ = Phase::Idle;
    steps.finish(action_);
}
void ShutdownSequence::refuse(Steps &steps, bool stopped) {
    phase_ = Phase::Idle;
    const bool resumed = !stopped || steps.resume();
    steps.notice(!resumed ? "Hardware stop failed; restart required" : summary(action_, problems_));
}
const char *ShutdownSequence::summary(ShutdownAction action, uint32_t problems) {
    if (!problems) return nullptr;
    if (action == ShutdownAction::Restart) {
        if (problems & (ThemeNotSaved | SettingsNotSaved)) return "Restart refused: settings not saved; power off or Recovery";
        return "Restart refused: storage not finished; power off or Recovery";
    }
    const bool recovery = action == ShutdownAction::Recovery;
    if (problems & (ThemeNotSaved | SettingsNotSaved))
        return recovery ? "Settings not saved; opening Recovery" : "Settings not saved; powering off";
    if (problems & (StorageNotFinished | AddressingNotRestored))
        return recovery ? "Storage not finished; opening Recovery" : "Storage not finished; powering off";
    if (problems & FilesCancelled) return recovery ? "Recording cancelled; opening Recovery" : "Recording cancelled; powering off";
    return recovery ? "Screenshot not finished; opening Recovery" : "Screenshot not finished; powering off";
}
} // namespace openosc
