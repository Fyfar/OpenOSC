#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc::dmm {
class Packet {
public:
    static constexpr size_t Bytes = 18;
    static uint8_t checksum(const uint8_t *prefix);
    static bool valid(const uint8_t *data, size_t size);
};
class PacketFramer {
public:
    bool push(uint8_t byte, uint8_t (&output)[Packet::Bytes]);
    void reset() {
        count_ = 0;
        rejected_ = 0;
    }
    uint32_t rejected() const { return rejected_; }

private:
    uint8_t pending_[Packet::Bytes]{};
    size_t count_ = 0;
    uint32_t rejected_ = 0;
};
} // namespace openosc::dmm
