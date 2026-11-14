#include "openosc/settings/json_input.hpp"

namespace openosc {
namespace {
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
}
bool JsonInput::completeObject(const char* text, size_t bytes, unsigned maxDepth) {
    if (!text) return false;
    size_t i = 0;
    while (i < bytes && space(text[i])) ++i;
    if (i == bytes || text[i] != '{') return false;
    unsigned depth = 0;
    bool string = false, escape = false;
    for (; i < bytes; ++i) {
        const char c = text[i];
        if (string) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') string = false;
            continue;
        }
        if (c == '"') string = true;
        else if (c == '{' || c == '[') { if (++depth > maxDepth) return false; }
        else if (c == '}' || c == ']') {
            if (!depth) return false;
            if (!--depth) {
                while (++i < bytes) if (!space(text[i])) return false;
                return true;
            }
        }
    }
    return false;
}
}
