#pragma once

namespace openosc::calibration {
class ZeroFile;
class PwmFile;
class ScopeCalibration;
// "Use factory zero": both user zeros off (each file keeps its values, enabled=false), the factory
// offset PWM and zero codes for new captures. Only from values read from the disk; the legacy
// software zero first, since under a hardware zero it is shadowed (turning it off changes nothing
// in force), the hardware zero last, so a failed write never leaves a mixture in force.
class ZeroSelection {
public:
    enum class Result {
        Factory,     // Both off, saved and in force.
        Unavailable, // Not now (a calibration runs, no factory record): nothing touched.
        Unread,      // A file was not read (or not since a failed read): refused, nothing written.
        Unchanged,   // The first needed write failed: everything as it was.
        HardwareKept // Software zero off, the hardware zero's write failed: it stays in force (the
                     // correction in force is the one before), its file as it was.
    };
    static Result useFactory(ZeroFile &software,PwmFile &hardware,ScopeCalibration &calibration);
};
} // namespace openosc::calibration
