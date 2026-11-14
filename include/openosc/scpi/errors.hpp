#pragma once
#include <cstdint>

namespace openosc::scpi {
class ErrorQueue {
public:
    void push(int16_t code);
    int16_t pop();
    void clear() { head_ = count_ = 0; event_ = 0; }
    bool empty() const { return !count_; }
    uint8_t event() const { return event_; }
    uint8_t readEvent() { const auto value = event_; event_ = 0; return value; }
    void complete() { event_ |= 1; }
    static const char *message(int16_t code);
private:
    int16_t errors_[8]{};
    unsigned head_ = 0, count_ = 0;
    uint8_t event_ = 0;
};
} // namespace openosc::scpi
