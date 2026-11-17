#pragma once
#include "openosc/decode/protocols/can_fd.hpp"

// CAN (plan §8.5): Classical CAN and ISO CAN FD are one automaton, CanFdDecoder (can_fd.hpp). These names
// are that automaton, not a second implementation.
namespace openosc::decoding::protocols {
using CanOptions = CanFdOptions;
using CanDecoder = CanFdDecoder;
} // namespace openosc::decoding::protocols
