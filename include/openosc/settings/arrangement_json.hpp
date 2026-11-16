#pragma once
#include "openosc/settings/screen_arrangement.hpp"
#include <ArduinoJson.h>

namespace openosc {
// A block order: an array of its keys, top to bottom. Anything else (not an array, a wrong length,
// an unknown or doubled key) leaves the default: a damaged order never rejects the file.
template<class Block> inline void arrangement(JsonObjectConst object, const char *name, BlockOrder<Block> &output) {
    constexpr unsigned Size = BlockOrder<Block>::Size;
    const auto array = object[name].as<JsonArrayConst>();
    if (array.isNull() || array.size() != Size) return;
    const char *keys[Size];
    for (unsigned i = 0; i < Size; ++i) {
        if (!array[i].is<const char *>()) return;
        keys[i] = array[i].as<const char *>();
    }
    BlockOrder<Block>::fromKeys(keys, Size, output);
}
template<class Block> inline void arrangement(JsonObject object, const char *name, const BlockOrder<Block> &values) {
    auto array = object.createNestedArray(name);
    for (unsigned slot = 0; slot < BlockOrder<Block>::Size; ++slot) array.add(BlockOrder<Block>::key(values.at(slot)));
}
} // namespace openosc
