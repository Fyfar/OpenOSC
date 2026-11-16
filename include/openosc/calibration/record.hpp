#pragma once
#include <cstdint>
#include "openosc/calibration/data.hpp"

namespace openosc {
// The factory record in internal Flash (0x0801C000), read only: its words as stored and its decoding
// through CalibrationData's one decoder (the application with its boot policy, Recovery always Auto).
class CalibrationRecord {
public:
    static constexpr unsigned Channels = 2, Rows = 12, Columns = 7;
    static constexpr unsigned RecordBytes = 346;
    uint16_t value(unsigned channel, unsigned row, unsigned column) const;
    uint16_t word(unsigned offset) const;
    bool load(CalibrationData& output, FactoryRecordPolicy policy = FactoryRecordPolicy::Auto) const;
};
}
