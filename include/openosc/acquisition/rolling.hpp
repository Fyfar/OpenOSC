#pragma once
#include "openosc/acquisition/frame.hpp"

namespace openosc::scope {
class RollingBuffer {
public:
    explicit RollingBuffer(uint16_t *storage, size_t capacity) : storage_(storage), capacity_(capacity), limit_(capacity) {}
    bool append(const FrameView &frame);
    bool code(Channel channel, size_t index, uint8_t &output) const;
    FrameView snapshot() const;
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool reset(size_t capacity) {
        if (!capacity || capacity>65536 || capacity>limit_) return false;
        capacity_=capacity; reset(); return true;
    }
    uint64_t dropped() const { return dropped_; }
    void reset() {
        cursor_ = size_ = 0;
        dropped_ = 0;
        continuous_ = false;
    }

private:
    uint16_t *storage_;
    size_t capacity_, limit_, cursor_ = 0, size_ = 0;
    uint64_t dropped_ = 0;
    FrameMetadata metadata_;
    uint64_t nextTimestampUs_ = 0;
    bool continuous_ = false;
};
} // namespace openosc::scope
