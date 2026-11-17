#pragma once
#include "openosc/acquisition/frame.hpp"

namespace openosc::decoding {
// Which record a result belongs to (plan §5.6): a generation (raised when the decoder starts, when the
// record sequence goes back - a new capture session - and for each loaded record; not for every
// capture of a RUN), the record's sequence, timestamp, origin and samples, and a fingerprint of the
// metadata that changes what its samples mean (rate, trigger, layout, channels, scales, probe,
// calibration and interleave corrections). Two records equal in all of it are the same record.
struct RecordKey {
    uint32_t generation = 0, sequence = 0;
    uint64_t timestampUs = 0;
    uint32_t origin = 0, samples = 0, fingerprint = 0;
    bool valid = false;
    static RecordKey of(const scope::FrameMetadata &metadata, uint32_t generation);
    bool operator==(const RecordKey &o) const {
        return valid == o.valid && generation == o.generation && sequence == o.sequence && timestampUs == o.timestampUs &&
               origin == o.origin && samples == o.samples && fingerprint == o.fingerprint;
    }
    bool operator!=(const RecordKey &o) const { return !(*this == o); }
};
} // namespace openosc::decoding
