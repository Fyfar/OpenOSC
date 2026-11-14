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
uint16_t CalibrationRecord::storedCrc() const { return word(DataBytes); }
bool CalibrationRecord::load(CalibrationData& output) const {
    uint8_t bytes[RecordBytes];
    auto* source = reinterpret_cast<volatile const uint8_t*>(layout::CalibrationBase);
    for (unsigned i = 0; i < RecordBytes; ++i) bytes[i] = source[i];
    return output.load(bytes, sizeof bytes);
}
uint16_t CalibrationRecord::computedCrc() const {
    uint16_t crc = 0xffff;
    auto* bytes = reinterpret_cast<volatile const uint8_t*>(layout::CalibrationBase);
    for (unsigned i = 0; i < DataBytes; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);
    }
    return crc;
}
}

