#pragma once

namespace openosc {
enum class RecoveryAction { Update, Calibration, Backup, CheckFat, Launch, PowerOff, Format, Count };
inline constexpr const char* RecoveryItems[] = {
    "UPDATE FIRMWARE", "VIEW CALIBRATION", "BACKUP FACTORY BOOT",
    "CHECK / REPAIR FAT", "START APPLICATION", "POWER OFF", "FORMAT EXTERNAL DISK"
};
inline constexpr unsigned RecoveryItemCount = static_cast<unsigned>(RecoveryAction::Count);
static_assert(sizeof RecoveryItems / sizeof *RecoveryItems == RecoveryItemCount);
}
