#pragma once
#include "openosc/calibration/data.hpp"
#include "openosc/calibration/zero_profile.hpp"

namespace openosc::calibration {
// The hardware zero: the offset PWM compare of every channel, range 0..10 and node Y = 20..140
// (ZeroProfile::Positions) measured so that a grounded input reads the zero code its metadata
// states. It replaces the factory nodes (interpolated alike); the factory record stays read-only.
struct PwmProfile {
    static constexpr unsigned Channels = ZeroProfile::Channels, Ranges = ZeroProfile::Ranges, Points = ZeroProfile::Points;
    // The hardware zero's own limit, not the software zero's 16 codes: how far a node may lie from
    // the factory one, in codes of zero at the factory slope. A grounded input reads (80 - Y) / ppc
    // codes, about +-80 at the end nodes Y = 20/140 (ppc 0.75); every sample must stay inside
    // (-125, +125) to be measured, so an offset of up to ~40 codes is still measurable there with
    // its noise. 40 codes is 30 pixels, 1.5 divisions: a factory table that far off is corrected; a
    // larger offset (or a DC input that large) is refused. A DC input below it cannot be told from
    // an offset: the inputs must be grounded (the confirmation says so).
    static constexpr double MaximumShiftCodes = 40;
    uint16_t factoryCrc = 0;
    bool enabled = false;
    uint16_t compare[Channels][Ranges][Points]{};
    // Bound to this factory record (CRC), every node a compare, every row strictly monotonic in the
    // factory row's direction, and no node further from the factory one than MaximumShiftCodes at
    // the factory slope.
    // A disabled profile that never held values (all nodes 0) is valid too: "Use factory zero"
    // before any hardware zero.
    bool valid(const CalibrationData &factory) const;
    bool empty() const;
};
class PwmGeometry {
public:
    // Codes of zero per compare count at node `point` of a channel and range: the factory slope of
    // its segment (the last node takes the last segment's), from the factory zero codes of the two
    // nodes (CalibrationData::scopeScale) and their factory compares. False when the factory row is
    // not strictly monotonic or the record cannot scale (the hardware zero then cannot be found).
    static bool slope(const CalibrationData &factory, unsigned channel, unsigned range, unsigned point, double &codesPerCount);
    // The zero code a grounded input should read at a node (the factory transfer).
    static bool target(const CalibrationData &factory, unsigned channel, unsigned range, unsigned point, double &code);
};
} // namespace openosc::calibration
