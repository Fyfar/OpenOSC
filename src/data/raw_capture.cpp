#include "openosc/data/raw_capture.hpp"
#include "openosc/data/binary.hpp"
#include "openosc/update/crc32.hpp"
#include <cstring>

namespace openosc {
namespace {
constexpr uint8_t magic[] = {'O', 'S', 'C', 'R', 'A', 'W', '1', 0};
void putDouble(uint8_t *p, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    Binary::writeLe(p, bits, 8);
}
double getDouble(const uint8_t *p) {
    uint64_t bits = Binary::readLe(p, 8);
    double value;
    std::memcpy(&value, &bits, 8);
    return value;
}
unsigned channels(const scope::FrameMetadata &m) {
    return unsigned(m.channels[0].enabled) + unsigned(m.channels[1].enabled);
}
} // namespace
bool RawCaptureCodec::encode(const scope::FrameView &frame, uint32_t crc, uint8_t *out, size_t capacity) {
    if (!frame.valid() || !out || capacity < HeaderBytes || !channels(frame.metadata()))
        return false;
    const auto &m = frame.metadata();
    std::memset(out, 0, HeaderBytes);
    std::memcpy(out, magic, 8);
    // Version 4 only for an InterleavedCh1 record: its payload holds the codes as read (the HIGH
    // ADC's correction applied, FrameView::code), and the header keeps the correction they were
    // read with (bytes 144..147) and that fact (byte 148 = 1). Every other record stays version 3.
    const bool interleaved = m.layout == scope::Layout::InterleavedCh1;
    Binary::writeLe(out + 8, HeaderBytes, 2);
    Binary::writeLe(out + 10, interleaved ? 4 : 3, 2);
    Binary::writeLe(out + 12, m.samples * channels(m), 4);
    Binary::writeLe(out + 16, m.samples, 4);
    Binary::writeLe(out + 20, m.ring, 4);
    Binary::writeLe(out + 24, m.origin, 4);
    Binary::writeLe(out + 28, m.sequence, 4);
    Binary::writeLe(out + 32, m.timestampUs, 8);
    Binary::writeLe(out + 40, m.samplesPerSecond.numerator, 4);
    Binary::writeLe(out + 44, m.samplesPerSecond.denominator, 4);
    out[48] = static_cast<uint8_t>(m.layout);
    out[49] = unsigned(m.timingVerified) | (unsigned(m.triggered) << 1) | (unsigned(m.rolling)<<2);
    for (unsigned i = 0; i < 2; ++i) {
        const auto &c = m.channels[i];
        if (c.enabled)
            out[50] |= 1U << i;
        auto *p = out + 52 + i * 40;
        Binary::writeLe(p, c.millivoltsPerDivision, 4);
        Binary::writeLe(p + 4, c.probeNumerator, 4);
        Binary::writeLe(p + 8, c.probeDenominator, 4);
        putDouble(p + 12, c.voltsPerCode);
        putDouble(p + 20, c.zeroCode);
        Binary::writeLe(p + 28, c.calibrationCrc, 2);
        p[30] = static_cast<uint8_t>(c.coupling);
        p[31] = c.calibrated;
        Binary::writeLe(p + 32, uint16_t(c.analogPosition), 2);
    }
    Binary::writeLe(out + 132, m.triggerIndex, 4);
    Binary::writeLe(out + 136, m.timebaseNs, 8);
    if (interleaved) {
        uint32_t bits;
        std::memcpy(&bits, &m.interleaveOffset, 4);
        Binary::writeLe(out + 144, bits, 4);
        out[148] = 1;
    }
    Binary::writeLe(out + 152, crc, 4);
    Binary::writeLe(out + 156, Crc32::compute(out, 156), 4);
    return true;
}
bool RawCaptureCodec::header(const uint8_t *in, size_t bytes, RawCaptureHeader &output) {
    const auto version = in && bytes >= HeaderBytes ? Binary::readLe(in + 10, 2) : 0;
    if (!in || bytes < HeaderBytes || std::memcmp(in, magic, 8) || Binary::readLe(in + 8, 2) != HeaderBytes ||
        version < 1 || version > 4 || Binary::readLe(in + 156, 4) != Crc32::compute(in, 156) ||
        !in[50] || (in[50] & ~3U) || (in[49] & ~7U) || in[51])
        return false;
    RawCaptureHeader result;
    auto &m = result.metadata;
    result.payloadBytes = Binary::readLe(in + 12, 4);
    result.payloadCrc = Binary::readLe(in + 152, 4);
    m.samples = Binary::readLe(in + 16, 4);
    m.ring = Binary::readLe(in + 20, 4);
    m.origin = Binary::readLe(in + 24, 4);
    m.sequence = Binary::readLe(in + 28, 4);
    m.timestampUs = Binary::readLe(in + 32, 8);
    m.samplesPerSecond = {uint32_t(Binary::readLe(in + 40, 4)), uint32_t(Binary::readLe(in + 44, 4))};
    m.layout = static_cast<scope::Layout>(in[48]);
    m.timingVerified = in[49] & 1;
    m.triggered = in[49] & 2;
    m.rolling=in[49]&4;
    for (unsigned i = 0; i < 2; ++i) {
        const auto *p = in + 52 + i * 40;
        auto &c = m.channels[i];
        c.millivoltsPerDivision = Binary::readLe(p, 4);
        c.probeNumerator = Binary::readLe(p + 4, 4);
        c.probeDenominator = Binary::readLe(p + 8, 4);
        c.voltsPerCode = getDouble(p + 12);
        c.zeroCode = getDouble(p + 20);
        c.calibrationCrc = Binary::readLe(p + 28, 2);
        if (p[30] > 1 || p[31] > 1)
            return false;
        c.coupling = static_cast<scope::Coupling>(p[30]);
        c.calibrated = p[31];
        c.enabled = in[50] & (1U << i);
        // Older files did not record the analog zero; their traces were drawn as if it were centred.
        if (version >= 3) {
            const auto position = Binary::readLe(p + 32, 2);
            if (position > 160) return false;
            c.analogPosition = int16_t(position);
        }
        for (unsigned j = version >= 3 ? 34 : 32; j < 40; ++j)
            if (p[j])
                return false;
    }
    m.triggerIndex = Binary::readLe(in + 132, 4);
    const bool extended = version >= 2;
    if (extended) m.timebaseNs=Binary::readLe(in+136,8);
    if (version == 2 && !m.timebaseNs) return false;
    unsigned reserved = extended ? 144 : 136;
    if (version == 4) { // An InterleavedCh1 record with its codes already corrected.
        uint32_t bits = uint32_t(Binary::readLe(in + 144, 4));
        std::memcpy(&m.interleaveOffset, &bits, 4);
        if (m.layout != scope::Layout::InterleavedCh1 || in[148] != 1 || !scope::InterleaveCorrection::valid(m.interleaveOffset))
            return false;
        reserved = 149;
    }
    for (unsigned j = reserved; j < 152; ++j)
        if (in[j])
            return false;
    if (!m.valid() || result.payloadBytes != m.samples * channels(m))
        return false;
    output = result;
    return true;
}
bool RawCaptureCodec::decode(const uint8_t *in, size_t bytes, uint16_t *words, size_t capacity,
                             scope::FrameView &output) {
    RawCaptureHeader h;
    if (!words || !header(in, bytes, h) || capacity < h.metadata.samples ||
        bytes != HeaderBytes + h.payloadBytes ||
        Crc32::compute(in + HeaderBytes, h.payloadBytes) != h.payloadCrc)
        return false;
    if (Binary::overlaps(in, bytes, words, size_t(h.metadata.samples) * sizeof(*words)))
        return false;
    size_t cursor = HeaderBytes;
    for (size_t i = 0; i < h.metadata.samples; ++i) {
        uint16_t word = 0;
        for (unsigned c = 0; c < 2; ++c)
            if (h.metadata.channels[c].enabled)
                word |= uint16_t(in[cursor++]) << (c * 8);
        words[i] = word;
    }
    // The payload holds every point as read (an InterleavedCh1 record's correction already in its
    // codes): a flat Linear record, read with no correction again.
    h.metadata.layout = scope::Layout::Linear;
    h.metadata.interleaveOffset = 0;
    h.metadata.ring = h.metadata.samples;
    h.metadata.origin = 0;
    const uint16_t *banks[4] = {words, nullptr, nullptr, nullptr};
    output = scope::FrameView(h.metadata, banks, capacity);
    return output.valid();
}
bool RawCaptureStream::begin(const scope::FrameView &frame) {
    if (!frame.valid() || !channels(frame.metadata()))
        return false;
    Crc32 crc;
    for (size_t i = 0; i < frame.metadata().samples; ++i)
        for (unsigned c = 0; c < 2; ++c)
            if (frame.metadata().channels[c].enabled) {
                uint8_t value;
                if (!frame.code(static_cast<scope::Channel>(c), i, value))
                    return false;
                crc.update(&value, 1);
            }
    if (!RawCaptureCodec::encode(frame, crc.value(), header_, sizeof header_))
        return false;
    frame_ = frame;
    channels_ = channels(frame.metadata());
    bytes_ = sizeof header_ + frame.metadata().samples * channels_;
    return true;
}
size_t RawCaptureStream::read(size_t offset, uint8_t *output, size_t capacity) const {
    if (!output || offset >= bytes_ || !bytes_)
        return 0;
    size_t count = bytes_ - offset;
    if (count > capacity)
        count = capacity;
    for (size_t i = 0; i < count; ++i) {
        const size_t position = offset + i;
        if (position < sizeof header_) {
            output[i] = header_[position];
            continue;
        }
        const size_t raw = position - sizeof header_, sample = raw / channels_;
        const unsigned channel = channels_ == 2 ? raw % 2 : frame_.metadata().channels[0].enabled ? 0 : 1;
        uint8_t value;
        if (!frame_.code(static_cast<scope::Channel>(channel), sample, value))
            return 0;
        output[i] = value;
    }
    return count;
}
} // namespace openosc
