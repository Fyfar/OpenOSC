#pragma once
#include "openosc/ui/scope/options.hpp"
#include <ArduinoJson.h>

namespace openosc {
class PlotJson {
public:
    static bool parse(JsonObjectConst object,plot::Options& output);
    static void encode(JsonObject object,const plot::Options& options);
};
}
