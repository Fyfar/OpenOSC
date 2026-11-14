#pragma once
#include "openosc/acquisition/frame.hpp"
#include <cstddef>

namespace openosc {
struct RawCaptureHeader {
    scope::FrameMetadata metadata;
    uint32_t payloadBytes = 0, payloadCrc = 0;
};
class RawCaptureCodec {
public:
    static constexpr size_t HeaderBytes = 160;
    static bool encode(const scope::FrameView &frame, uint32_t payloadCrc, uint8_t *header, size_t capacity);
    static bool header(const uint8_t *input, size_t bytes, RawCaptureHeader &output);
    static bool decode(const uint8_t *input, size_t bytes, uint16_t *words, size_t capacity,
                       scope::FrameView &output);
};
class RawCaptureStream {
public:
    bool begin(const scope::FrameView &frame);
    size_t size() const { return bytes_; }
    size_t read(size_t offset, uint8_t *output, size_t capacity) const;

private:
    scope::FrameView frame_;
    uint8_t header_[RawCaptureCodec::HeaderBytes]{};
    unsigned channels_ = 0;
    size_t bytes_ = 0;
};
} // namespace openosc
