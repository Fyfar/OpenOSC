#include "openosc/calibration/record.hpp"
#include "openosc/update/layout.hpp"

namespace openosc {
uint16_t CalibrationRecord::word(unsigned offset) const {
    if (offset >= RecordBytes || (offset & 1)) return 0;
    return *reinterpret_cast<volatile const uint16_t*>(layout::CalibrationBase + offset);
}
uint16_t CalibrationRecord::value(unsigned channel, unsigned row, unsigned column) const {
    if (channel >= Channels || row >= Rows || column >= Columns) return 0;
    return word(2 * ((channel * Rows + row) * Columns + column));
}
bool CalibrationRecord::load(CalibrationData& output, FactoryRecordPolicy policy) const {
    uint8_t bytes[RecordBytes];
    auto* source = reinterpret_cast<volatile const uint8_t*>(layout::CalibrationBase);
    for (unsigned i = 0; i < RecordBytes; ++i) bytes[i] = source[i];
    return output.load(bytes, sizeof bytes, policy);
}
}
