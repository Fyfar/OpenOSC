#pragma once
#include "openosc/measurement/dmm/function.hpp"

namespace openosc::dmm {
class Terminals {
public:
    static const char *forFunction(Function function) {
        if (function==Function::CurrentDc || function==Function::CurrentAc) return "COM+10A";
        if (function==Function::MilliampsDc || function==Function::MilliampsAc) return "COM+mA";
        return function<Function::Unknown ? "COM+V" : "";
    }
};
} // namespace openosc::dmm
