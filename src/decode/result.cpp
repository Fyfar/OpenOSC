#include "openosc/decode/result.hpp"

namespace openosc::decoding {
void ResultSet::reset(const RecordKey &key, uint32_t settingsHash, Protocol protocol) {
    key_ = key; settingsHash_ = settingsHash; protocol_ = protocol;
    count_ = used_ = 0; lost_ = 0;
    processed = samples = glitches = skipped = cycles = dropped = overflows = clipped = 0; flagsSeen = 0;
    status = DecodeStatus::Decoding; initialLevel = -1; channel = 0;
    samplesPerSecond = {}; triggerIndex = 0; triggered = false; window = 1;
    lines = 0; difference = hysteresisAuto = levelsKnown = measuring = false;
    thresholdMv[0] = thresholdMv[1] = 0; hysteresisMv[0] = hysteresisMv[1] = 0;
}
bool ResultSet::append(const DecodeEvent &event) {
    if (count_ >= Events) { ++lost_; return false; }
    events_[count_++] = event;
    return true;
}
uint16_t ResultSet::reserve(unsigned bytes) {
    if (bytes > Pool - used_ || count_ >= Events) return 0xFFFF;
    const uint16_t offset = uint16_t(used_);
    used_ += bytes;
    return offset;
}
uint16_t ResultSet::extend(unsigned bytes, unsigned width) {
    if (!count_) return 0xFFFF;
    const auto &e = events_[count_ - 1];
    if (e.payload == 0xFFFF || unsigned(e.payload) + unsigned(e.count) * width != used_ || bytes > Pool - used_) return 0xFFFF;
    const uint16_t offset = uint16_t(used_);
    used_ += bytes;
    return offset;
}
bool ResultSets::acquire(ResultLease &out) {
    if (!hasPublished_ || held()) return false;
    leased_ = uint8_t(published_);
    out = {leased_, ++ticket_};
    return true;
}
void ResultSets::release(const ResultLease &lease) {
    if (lease.valid() && lease.slot == leased_ && lease.ticket == ticket_) leased_ = 0xFF;
}
const ResultSet *ResultSets::leased(const ResultLease &lease) const {
    return lease.valid() && lease.slot == leased_ && lease.ticket == ticket_ ? &sets_[leased_] : nullptr;
}
} // namespace openosc::decoding
