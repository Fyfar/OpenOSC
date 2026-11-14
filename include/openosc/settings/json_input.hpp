#pragma once
#include <cstddef>

namespace openosc {
class JsonInput {
public:
    static bool completeObject(const char* text, size_t bytes, unsigned maxDepth);
};
}
