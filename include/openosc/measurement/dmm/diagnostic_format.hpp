#pragma once
#include "openosc/measurement/dmm/result.hpp"

namespace openosc::dmm {
class DiagnosticFormat {
public:
    // Factory 703S diagnostic syntax. Intentionally no line terminator.
    static size_t format(const Result &result,char *output,size_t capacity);
};
} // namespace openosc::dmm
