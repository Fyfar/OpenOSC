#include "openosc/application/boot_guard.hpp"
#include "openosc/settings/run_mode.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/filesystem.hpp"
#include "openosc/storage/json_files.hpp"
#include "openosc/storage/layout.hpp"
#include <cstring>

namespace openosc {
namespace {
// The only contents the guard writes; anything else is not a valid guard file (Pending).
constexpr const char Starting[] = "{\"profile\":2,\"state\":\"starting\"}";
constexpr const char Confirmed[] = "{\"profile\":2,\"state\":\"ok\"}";
constexpr const char Stopped[] = "{\"profile\":2,\"state\":\"stopped\"}";
static_assert(OverclockedRunMode == 2, "The guard file names profile 2");
bool equal(const char *json, size_t bytes, const char *text) { return bytes == std::strlen(text) && !std::memcmp(json, text, bytes); }
} // namespace

// Read only (JsonFiles::read): unlike the theme and calibration profiles, nothing is archived.
BootGuard::State BootGuard::read() {
    if (!fs_.mounted()) return State::Unreadable;
    char json[64];
    size_t bytes = 0;
    switch (JsonFiles::read(storage::Layout::BootGuardFile, json, sizeof json, bytes)) {
    case JsonFiles::ReadResult::StorageError: return State::Unreadable;
    case JsonFiles::ReadResult::BadSize: return State::Pending;
    case JsonFiles::ReadResult::Complete:
        return equal(json, bytes, Confirmed) || equal(json, bytes, Stopped) ? State::Clear : State::Pending;
    case JsonFiles::ReadResult::Missing: break;
    }
    // No current file: only a previous one means a write was cut between its renames.
    switch (JsonFiles::read(storage::Layout::BootGuardPrevious, json, sizeof json, bytes)) {
    case JsonFiles::ReadResult::Missing: return State::Clear;
    case JsonFiles::ReadResult::StorageError: return State::Unreadable;
    default: return State::Pending;
    }
}
bool BootGuard::write(const char *text) {
    return fs_.mounted() && storage::ensureDirectory(storage::Layout::Settings) == FR_OK &&
           JsonFiles::write(storage::Layout::BootGuardFile, text, std::strlen(text));
}
bool BootGuard::arm() { return write(Starting); }
bool BootGuard::confirm() { return write(Confirmed); }
bool BootGuard::stop() { return write(Stopped); }

BootGuard::Start BootGuard::start(uint32_t requested, Setting &setting) {
    constexpr const char *Unavailable = "360 MHz not started: boot guard unavailable; 200 MHz active";
    if (requested != OverclockedRunMode) return {requested, false, nullptr};
    switch (read()) {
    case State::Clear: return arm() ? Start{requested, true, nullptr} : Start{0, false, Unavailable};
    case State::Unreadable: return {0, false, Unavailable}; // Not evidence of a failed start: nothing written.
    case State::Pending: break;
    }
    // The last start on 360 was not confirmed: never 360 now.
    if (!setting.saveRunMode(0)) return {0, false, "360 MHz start not confirmed; 200 MHz active"};
    stop(); // Safe only now: the setting already says 200. A failed write leaves a harmless marker.
    return {0, false, "360 MHz start not confirmed; run mode set to 200 MHz"};
}
} // namespace openosc
