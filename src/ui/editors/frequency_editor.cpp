#include "openosc/ui/editors/frequency_editor.hpp"
#include <algorithm>
#include <cinttypes>
#include <cstdio>

namespace openosc::ui {
void FrequencyEditor::begin(uint64_t millihertz, uint64_t maximum, uint64_t minimum) {
    maximum_ = std::clamp<uint64_t>(maximum, 1, 10000000000ULL);
    minimum_ = std::clamp<uint64_t>(minimum, 1, maximum_);
    value_ = std::clamp<uint64_t>(millihertz, minimum_, maximum_);
    power_ = 3;
    unit_ = value_ >= 1000000000 ? 2 : value_ >= 1000000 ? 1 : 0;
}
uint64_t FrequencyEditor::step() const {
    uint64_t result = 1;
    for (unsigned i = 0; i < power_; ++i) result *= 10;
    return result;
}
void FrequencyEditor::move(int direction) {
    if (!direction) return;
    power_ = unsigned(std::clamp<int>(int(power_) + (direction < 0 ? 1 : -1), 0, int(digits()) - 1));
}
void FrequencyEditor::adjust(int direction) {
    if (!direction) return;
    const int64_t next = int64_t(value_) + int64_t(step()) * (direction > 0 ? 1 : -1);
    value_ = uint64_t(std::clamp<int64_t>(next, int64_t(minimum_), int64_t(maximum_)));
}
void FrequencyEditor::text(char *output, size_t capacity) const {
    uint64_t scale = 1000;
    for (unsigned i = 0; i < unit_; ++i) scale *= 1000;
    std::snprintf(output, capacity, "%0*" PRIu64 ".%0*" PRIu64, int(integerDigits()), value_ / scale, int(3 + unit_ * 3), value_ % scale);
}
} // namespace openosc::ui
