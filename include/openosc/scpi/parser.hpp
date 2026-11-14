#pragma once
#include <cstddef>
#include <cstdint>

namespace openosc::scpi {
struct Command {
    char header[96]{}, argument[160]{};
    int16_t error = 0;
    bool query = false;
};
class Parser {
public:
    bool feed(uint8_t byte); // False applies backpressure; retry the same byte.
    bool take(Command &command);
    void reset();
    bool endMessage() { return feed('\n'); }
private:
    bool finish(bool end);
    void decode(Command &command);
    char line_[256]{}, path_[96]{};
    size_t used_ = 0;
    Command commands_[4];
    unsigned head_ = 0, count_ = 0;
    char quote_ = 0;
    int16_t error_ = 0;
};
} // namespace openosc::scpi
