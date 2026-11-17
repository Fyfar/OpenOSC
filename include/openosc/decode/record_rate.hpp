#pragma once
#include <cstdint>

namespace openosc::decoding {
// The decoder's own pace for its info row (plan §5.3), apart from the display's FPS: results
// published a second (over windows of at least a second) and the age of the result on show. Fed the
// result sets' revision on every display pass; a clear (no result) is not a record.
class RecordRate {
public:
    void update(uint32_t revision, bool published, uint32_t nowMs);
    uint32_t perSecondX10() const { return rateX10_; }
    bool age(uint32_t nowMs, uint32_t &ms) const { ms = nowMs - publishedAt_; return published_; }
private:
    static constexpr uint32_t WindowMs = 1000;
    uint32_t revision_ = 0, publishedAt_ = 0, windowAt_ = 0, count_ = 0, rateX10_ = 0;
    bool seen_ = false, published_ = false;
};
} // namespace openosc::decoding
