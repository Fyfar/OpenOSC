#pragma once
#include "openosc/acquisition/frame.hpp"

// A stateless prefilter for the threshold stream (docs/SCOPE_PROTOCOL_DECODERS_DSP.md): whether a block
// of a record cannot move a Schmitt level, decided on the captured bytes with the core's SIMD byte
// compare (decode/simd.hpp: SSUB8/SEL on Cortex-M7, an exact emulation on the host).
namespace openosc::decoding::acceleration {
class PlateauScan {
public:
    static constexpr uint32_t MaxCount = 128;
    // How quiet() reads a record's channel: Simd straight from the banks (Linear, FourBanks and
    // FourByteBanks at any origin, ring size and wrap), Gather word by word through SplitHistory::word
    // (split FourBanks), None (InterleavedCh1, an invalid frame, a channel off): quiet() is always false.
    enum class Path : uint8_t { None, Simd, Gather };
    static Path path(const scope::FrameView &frame, scope::Channel channel);
    // True only when every canonical code (FrameView::signedCode) of the points [first, first + count)
    // of `channel` keeps the level: a low level stays while every code < high, a high one while every
    // code > low (ThresholdStream's rule). False whenever that is not proven: a code that would move
    // the level, any AdcBusGlitch suspect byte of the channel (0x7F/0x3F, whose canonical code depends on
    // its neighbours) or a false positive of the word-wide suspect test from the other channel, a layout
    // without a path, an invalid frame, a channel off, count 0 or above MaxCount, points past the record.
    // The caller keeps the stream's state (initial level, pending minimum pulse) and feeds the block
    // through the scalar path whenever this is false. Reads only the points asked for.
    static bool quiet(const scope::FrameView &frame, scope::Channel channel, uint32_t first, uint32_t count, int16_t low,
                      int16_t high, bool levelHigh);
};
} // namespace openosc::decoding::acceleration
