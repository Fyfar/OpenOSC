#pragma once
#include "openosc/measurement/dmm/result.hpp"

namespace openosc::dmm {
class RecordCodec {
public:
    static constexpr size_t Bytes = 176, HoldBytes = 178;
    static bool encode(const Result &result, uint8_t *output, size_t capacity);
    static bool decode(const uint8_t *input, size_t bytes, Result &output);
    static bool encodeHold(const Result &result, uint8_t *output, size_t capacity);
    static bool decodeHold(const uint8_t *input, size_t bytes, Result &output);
};
} // namespace openosc::dmm
