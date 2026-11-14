#include "openosc/measurement/dmm/packet.hpp"
#include <cstring>

namespace openosc::dmm {
uint8_t Packet::checksum(const uint8_t *p) {
    uint16_t sum = 0xff;
    for (unsigned i = 1; i <= 16; ++i) {
        sum += p[i];
        if (i != 16)
            sum = (sum & 255) + (sum >> 8);
    }
    return uint8_t(sum);
}
bool Packet::valid(const uint8_t *data, size_t size) {
    if (!data || size != Bytes || data[0] != 0xff || checksum(data) != data[17])
        return false;
    unsigned erased = 0;
    for (unsigned i = 0; i < 17; ++i)
        erased += data[i] == 0xff;
    return erased < 11;
}
bool PacketFramer::push(uint8_t byte, uint8_t (&output)[Packet::Bytes]) {
    if (!count_ && byte != 0xff)
        return false;
    pending_[count_++] = byte;
    if (count_ != sizeof pending_)
        return false;
    if (Packet::valid(pending_, count_)) {
        std::memcpy(output, pending_, sizeof pending_);
        count_ = 0;
        return true;
    }
    ++rejected_;
    size_t next = 1;
    while (next < count_ && pending_[next] != 0xff)
        ++next;
    count_ -= next;
    std::memmove(pending_, pending_ + next, count_);
    return false;
}
} // namespace openosc::dmm
