#pragma once
#include <cstdint>
#include "openosc/calibration/data.hpp"

namespace openosc {
class CalibrationRecord {
public:
    static constexpr unsigned Channels = 2, Rows = 12, Columns = 7;
    static constexpr unsigned DataBytes = 344, RecordBytes = 346;
    uint16_t value(unsigned channel, unsigned row, unsigned column) const;
    uint16_t word(unsigned offset) const;
    uint16_t storedCrc() const;
    uint16_t computedCrc() const;
    bool valid() const { return storedCrc() == computedCrc(); }
    bool load(CalibrationData& output) const;
};
}

