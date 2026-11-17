#include "openosc/decode/protocols/types.hpp"

namespace openosc::decoding::protocols {
bool BitClock::of(const Rational &fs, uint32_t baud, BitClock &out) {
    if (!fs.valid() || !baud || baud > 100000000u) return false;
    const uint64_t q16 = (uint64_t(fs.numerator) << 16) / (uint64_t(fs.denominator) * baud);
    if (q16 < (2ull << 16) || q16 > (1ull << 40)) return false;
    out.pointsQ16 = q16;
    return true;
}
uint32_t pointsOfNs(const Rational &fs, uint32_t ns) {
    if (!fs.valid()) return 0;
    const double points = double(ns) * fs.numerator / (double(fs.denominator) * 1e9);
    if (points >= 4294967295.0) return 0xFFFFFFFFu;
    const uint32_t whole = uint32_t(points);
    return whole + (double(whole) < points ? 1 : 0);
}

void EventWriter::begin(ResultSet &result, const DecodeEvent &header) {
    header_ = header;
    header_.payload = 0xFFFF; header_.count = 0;
    width_ = elementBytes(header.format);
    total_ = 0; ++group_;
    active_ = true; stored_ = false; segmented_ = false; lostUnit_ = exhausted_;
    if (lostUnit_) lose(result);
}
DecodeEvent &EventWriter::edit(ResultSet &result) { return stored_ ? *result.lastEvent() : header_; }
void EventWriter::lose(ResultSet &result) {
    // Counted once a unit: a lost descriptor by the set, a unit lost for the pool here.
    if (result.count() >= ResultSet::Events) result.append(header_);
    else ++dropped_;
    // The event stored last: an earlier segment of this very unit lost its rest (Cut), a whole
    // earlier unit is complete and only marks where the storage ended.
    if (!exhausted_)
        if (auto *previous = result.lastEvent())
            previous->flags |= uint16_t(Truncated | (segmented_ ? DecodeEvent::Cut : 0));
    exhausted_ = lostUnit_ = true;
}
// The unit's first element: reserve() (pool and a free descriptor for the event still to be appended),
// then append() of its header, checked. Every further element: extend() of the event stored last, the
// pool right after its elements (only the pool limits it: the 512th descriptor takes all its elements).
bool EventWriter::store(ResultSet &result, uint16_t value) {
    const uint16_t offset = stored_ ? result.extend(width_, width_) : result.reserve(width_);
    if (offset == 0xFFFF) return false;
    uint8_t *bytes = result.pool(offset);
    bytes[0] = uint8_t(value);
    if (width_ == 2) bytes[1] = uint8_t(value >> 8);
    if (!stored_) {
        header_.payload = offset; header_.count = 1;
        if (!result.append(header_)) return false; // Not after reserve(): counted lost, the unit with it.
        stored_ = true;
        return true;
    }
    ++result.lastEvent()->count;
    return true;
}
void EventWriter::push(ResultSet &result, uint16_t value, uint32_t first, uint32_t last) {
    if (!active_ || !width_) return;
    ++total_;
    if (lostUnit_) return;
    if (stored_ && result.lastEvent()->count == 255) { // The next segment of the same group.
        DecodeEvent next = *result.lastEvent(); // kind, field, aux (group), format carried over.
        next.first = first;
        next.flags = uint16_t((next.flags & ~SegmentFlags) | DecodeEvent::Continued);
        header_ = next; header_.payload = 0xFFFF; header_.count = 0;
        stored_ = false; segmented_ = true;
    }
    if (!stored_) header_.last = last;
    if (store(result, value)) { result.lastEvent()->last = last; return; }
    if (stored_) { // The pool ran out inside the unit: what is stored stays, its rest is lost.
        result.lastEvent()->flags |= uint16_t(Truncated | DecodeEvent::Cut);
        ++dropped_;
        exhausted_ = lostUnit_ = true;
        return;
    }
    lose(result);
}
void EventWriter::end(ResultSet &result, uint32_t last, uint16_t flags) {
    if (!active_) return;
    active_ = false;
    if (lostUnit_) return;
    if (stored_) {
        auto *e = result.lastEvent();
        e->flags |= flags; e->last = last;
        return;
    }
    header_.flags |= flags; header_.last = last;
    if (!result.append(header_)) { // Lost descriptor (counted by the set).
        if (auto *previous = result.lastEvent()) previous->flags |= Truncated;
        exhausted_ = true;
    }
}
bool EventWriter::block(ResultSet &result, const DecodeEvent &header, const uint8_t *bytes, unsigned count,
                        const uint8_t *trailer, unsigned trailerBytes) {
    if (active_ || count > 255) return false; // A unit still open, or more than one descriptor holds.
    header_ = header;
    header_.payload = 0xFFFF; header_.count = uint8_t(count);
    width_ = 1; total_ = count; ++group_;
    stored_ = false; segmented_ = false; lostUnit_ = exhausted_;
    if (lostUnit_) { lose(result); return false; }
    const unsigned total = count + trailerBytes;
    if (total) {
        const uint16_t offset = result.reserve(total); // Pool and a free descriptor, checked together.
        if (offset == 0xFFFF) { lose(result); return false; }
        uint8_t *pool = result.pool(offset);
        for (unsigned i = 0; i < count; ++i) pool[i] = bytes[i];
        for (unsigned i = 0; i < trailerBytes; ++i) pool[count + i] = trailer[i];
        header_.payload = offset;
    }
    if (!result.append(header_)) { // A full table (only without payload: reserve() checked it).
        if (auto *previous = result.lastEvent()) previous->flags |= Truncated;
        exhausted_ = lostUnit_ = true;
        return false;
    }
    return true;
}

bool CoincidenceQueue::pop(Item &out, Pop mode) {
    if (!count_) return false;
    const Transition &front = items_[head_];
    const bool open = items_[(head_ + count_ - 1) % Size].index <= front.index + window_;
    if (open && (mode == Pop::Ready || (mode == Pop::Full && count_ < Size))) return false;
    const bool forced = open && mode == Pop::Full; // The rest of its window is not seen yet.
    // Among equal indices the first channel's transition goes first (a fixed rule, flagged anyway).
    if (front.channel != first_)
        for (unsigned i = 1; i < count_; ++i) {
            Transition &other = items_[(head_ + i) % Size];
            if (other.index != front.index) break;
            if (other.channel == first_) { const Transition t = other; other = items_[head_]; items_[head_] = t; break; }
        }
    out.t = items_[head_];
    const unsigned mine = out.t.channel & 1u, other = mine ^ 1u;
    bool near = seen_[other] && out.t.index - lastIndex_[other] <= window_;
    for (unsigned i = 1; i < count_ && !near; ++i) {
        const Transition &t = items_[(head_ + i) % Size];
        if (t.index - out.t.index > window_) break;
        near = (t.channel & 1u) == other;
    }
    out.ambiguous = near || forced;
    if (forced) ++overflows_;
    lastIndex_[mine] = out.t.index; seen_[mine] = true;
    head_ = (head_ + 1) % Size; --count_;
    return true;
}
const Transition *CoincidenceQueue::pending(uint8_t channel, uint32_t index) const {
    for (unsigned i = 0; i < count_; ++i) {
        const Transition &t = items_[(head_ + i) % Size];
        if (t.index - index > window_) break;
        if (t.channel == channel) return &t;
    }
    return nullptr;
}
} // namespace openosc::decoding::protocols
