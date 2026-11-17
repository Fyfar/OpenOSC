#include "openosc/decode/record_key.hpp"
#include <cstring>

namespace openosc::decoding {
namespace {
struct Fnv {
    uint32_t hash = 2166136261u;
    template <class T> void add(const T &value) {
        unsigned char bytes[sizeof value];
        std::memcpy(bytes, &value, sizeof value);
        for (unsigned char b : bytes) { hash ^= b; hash *= 16777619u; }
    }
};
}
RecordKey RecordKey::of(const scope::FrameMetadata &m, uint32_t generation) {
    RecordKey key;
    key.generation = generation; key.sequence = m.sequence; key.timestampUs = m.timestampUs;
    key.origin = m.origin; key.samples = m.samples; key.valid = m.valid();
    Fnv f;
    f.add(m.samplesPerSecond.numerator); f.add(m.samplesPerSecond.denominator);
    f.add(m.triggerIndex); f.add(uint8_t(m.triggered)); f.add(uint8_t(m.layout)); f.add(m.ring);
    f.add(m.interleaveOffset);
    for (const auto &c : m.channels) {
        f.add(uint8_t(c.enabled)); f.add(c.voltsPerCode); f.add(c.zeroCode);
        f.add(c.probeNumerator); f.add(c.probeDenominator); f.add(c.calibrationCrc); f.add(uint8_t(c.calibrated));
    }
    key.fingerprint = f.hash;
    return key;
}
} // namespace openosc::decoding
