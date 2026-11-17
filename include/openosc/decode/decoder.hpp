#pragma once
#include "openosc/decode/result.hpp"
#include "openosc/decode/threshold.hpp"

namespace openosc::decoding {
// A protocol's state machine over the logic transitions of its sources (plan §5.1): start() for a
// record, feed() for each block's transitions in record order (its state kept between blocks), finish()
// at the record's end. Events go to the set being built; a full set counts them as lost.
class ProtocolDecoder {
public:
    virtual void start(ResultSet &result, int initialLevel) = 0;
    virtual void feed(const Transition *transitions, unsigned count, ResultSet &result) = 0;
    virtual void finish(uint32_t samples, ResultSet &result) = 0;
protected:
    ~ProtocolDecoder() = default;
};
} // namespace openosc::decoding
