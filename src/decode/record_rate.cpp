#include "openosc/decode/record_rate.hpp"

namespace openosc::decoding {
void RecordRate::update(uint32_t revision, bool published, uint32_t now) {
    if (!seen_) { seen_ = true; revision_ = revision; windowAt_ = now; }
    if (revision != revision_) {
        revision_ = revision;
        if (published) { ++count_; publishedAt_ = now; }
    }
    published_ = published;
    if (!published) rateX10_ = count_ = 0;
    const uint32_t elapsed = now - windowAt_;
    if (elapsed >= WindowMs) {
        rateX10_ = uint32_t((uint64_t(count_) * 10000 + elapsed / 2) / elapsed);
        count_ = 0; windowAt_ = now;
    }
}
} // namespace openosc::decoding
