#pragma once
#include <cstdint>
#include <cstring>

namespace openosc {
// The order of a screen's blocks, top to bottom, below the status bar (which is always on top).
// One block is the soft keys (Buttons): where it stands in the order is the only thing that moves
// them; their four equal zones are the same at the top, in the middle and at the bottom. Each
// enumerator's place is the default order, so a value-initialised order is the default.
enum class ScopeBlock : uint8_t { Measurements, Waveform, Buttons, Count };
enum class DigitalBlock : uint8_t { Reading, Measurements, Buttons, Count }; // DMM digital faces.
enum class AnalogBlock : uint8_t { Dial, Measurements, Reading, Buttons, Count }; // DMM analog dial.
// The generator's main screen: the set frequency (title, digits, unit, Applied), the preview (the
// waveform and its level line), the soft keys.
enum class GeneratorBlock : uint8_t { Frequency, Preview, Buttons, Count };

// A block's menu name and its file key (JSON).
struct ArrangementBlockInfo { const char *name, *key; };
template<class Block> struct ArrangementBlocks;
template<> struct ArrangementBlocks<ScopeBlock> {
    static constexpr ArrangementBlockInfo Info[] = {{"Measurements", "measurements"}, {"Waveform", "waveform"}, {"Buttons", "buttons"}};
};
template<> struct ArrangementBlocks<DigitalBlock> {
    static constexpr ArrangementBlockInfo Info[] = {{"Reading", "reading"}, {"Measurements", "measurements"}, {"Buttons", "buttons"}};
};
template<> struct ArrangementBlocks<GeneratorBlock> {
    static constexpr ArrangementBlockInfo Info[] = {{"Frequency", "frequency"}, {"Preview", "preview"}, {"Buttons", "buttons"}};
};
template<> struct ArrangementBlocks<AnalogBlock> {
    static constexpr ArrangementBlockInfo Info[] = {{"Dial", "dial"}, {"Measurements", "measurements"}, {"Reading", "reading"}, {"Buttons", "buttons"}};
};

// A fixed-size permutation of Block, no heap. Slots and block indices are both 0..Size-1. Every
// change keeps it a permutation: place() moves one block and shifts the ones between, so no block
// is ever doubled or lost; a menu row "Position N" is one Choice of the Size block names.
template<class Block> class BlockOrder {
public:
    static constexpr unsigned Size = unsigned(Block::Count);
    static_assert(Size >= 2 && Size <= 8 && sizeof ArrangementBlocks<Block>::Info / sizeof *ArrangementBlocks<Block>::Info == Size);

    constexpr BlockOrder() : slots_{} { for (unsigned i = 0; i < Size; ++i) slots_[i] = uint8_t(i); }
    static constexpr BlockOrder defaults() { return {}; }

    constexpr Block at(unsigned slot) const { return Block(slots_[slot]); } // slot < Size.
    constexpr unsigned slotOf(Block block) const {
        for (unsigned i = 0; i < Size; ++i) if (slots_[i] == uint8_t(block)) return i;
        return Size; // Only for an invalid order.
    }
    constexpr bool first(Block block) const { return slotOf(block) == 0; }
    constexpr bool last(Block block) const { return slotOf(block) == Size - 1; }

    // Puts `block` at `slot`; the blocks between its old and new slot shift by one towards the
    // old slot, the others stay. False (nothing changed) for a slot or block out of range.
    constexpr bool place(unsigned slot, Block block) {
        const unsigned from = slotOf(block);
        if (slot >= Size || from >= Size) return false;
        if (from < slot) for (unsigned i = from; i < slot; ++i) slots_[i] = slots_[i + 1];
        else for (unsigned i = from; i > slot; --i) slots_[i] = slots_[i - 1];
        slots_[slot] = uint8_t(block);
        return true;
    }

    // Every block exactly once (a value from a file or a host).
    constexpr bool valid() const {
        unsigned seen = 0;
        for (unsigned i = 0; i < Size; ++i) {
            if (slots_[i] >= Size || (seen >> slots_[i] & 1U)) return false;
            seen |= 1U << slots_[i];
        }
        return true;
    }
    // From Size keys, top to bottom; false (output unchanged) unless they are a permutation.
    static bool fromKeys(const char *const *keys, unsigned count, BlockOrder &output) {
        if (count != Size) return false;
        BlockOrder order;
        for (unsigned i = 0; i < Size; ++i) {
            const unsigned block = find(keys[i]);
            if (block >= Size) return false;
            order.slots_[i] = uint8_t(block);
        }
        if (!order.valid()) return false;
        output = order;
        return true;
    }

    static constexpr const char *name(Block block) { return ArrangementBlocks<Block>::Info[unsigned(block)].name; }
    static constexpr const char *key(Block block) { return ArrangementBlocks<Block>::Info[unsigned(block)].key; }
    static unsigned find(const char *key) { // A file key's block index; Size if none.
        for (unsigned i = 0; key && i < Size; ++i) if (!std::strcmp(key, ArrangementBlocks<Block>::Info[i].key)) return i;
        return Size;
    }

    constexpr bool operator==(const BlockOrder &b) const {
        for (unsigned i = 0; i < Size; ++i) if (slots_[i] != b.slots_[i]) return false;
        return true;
    }
    constexpr bool operator!=(const BlockOrder &b) const { return !(*this == b); }

private:
    uint8_t slots_[Size]; // slots_[slot]: the block index there, top to bottom.
};
using ScopeArrangement = BlockOrder<ScopeBlock>;
using DigitalArrangement = BlockOrder<DigitalBlock>;
using AnalogArrangement = BlockOrder<AnalogBlock>;
using GeneratorArrangement = BlockOrder<GeneratorBlock>;
} // namespace openosc
