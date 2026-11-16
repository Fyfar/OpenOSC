#include "openosc/calibration/gain_file.hpp"
#include "openosc/settings/json_input.hpp"
#include "openosc/settings/json_workspace.hpp"
#include "openosc/acquisition/ranges.hpp"
#include <ArduinoJson.h>

namespace openosc::calibration {
namespace {
constexpr size_t DocumentBytes=JSON_OBJECT_SIZE(5)+JSON_ARRAY_SIZE(2)+3*JSON_ARRAY_SIZE(11)+128;
}
bool GainJson::parse(const char *text,size_t bytes,const CalibrationData &factory,GainProfile &output) {
    if (bytes>MaxBytes || !JsonInput::completeObject(text,bytes,4)) return false;
    auto &document=JsonWorkspace::document<StaticJsonDocument<DocumentBytes>>(); // SRAM4, not the stack.
    if (deserializeJson(document,text,bytes,DeserializationOption::NestingLimit(4))) return false;
    const auto root=document.as<JsonObjectConst>();
    if (!root["schema"].is<unsigned>() || root["schema"].as<unsigned>()!=1 ||
        !root["enabled"].is<bool>() || !root["factory_crc16"].is<uint16_t>() ||
        root["factory_crc16"].as<uint16_t>()!=factory.crc()) return false;
    const auto channels=root["gain"].as<JsonArrayConst>();
    const auto scales=root["millivolts_per_division"].as<JsonArrayConst>();
    if (channels.size()!=GainProfile::Channels || scales.size()!=GainProfile::Ranges) return false;
    for (unsigned r=0;r<GainProfile::Ranges;++r)
        if (!scales[r].is<unsigned>() || scales[r].as<unsigned>()!=scope::VoltageRanges::millivolts(r)) return false;
    GainProfile candidate; candidate.factoryCrc=factory.crc(); candidate.enabled=root["enabled"];
    for (unsigned c=0;c<GainProfile::Channels;++c) {
        const auto ranges=channels[c].as<JsonArrayConst>();
        if (ranges.size()!=GainProfile::Ranges) return false;
        for (unsigned r=0;r<GainProfile::Ranges;++r) {
            if (!ranges[r].is<float>()) return false;
            candidate.gain[c][r]=ranges[r].as<float>();
        }
    }
    if (!candidate.valid(factory)) return false;
    output=candidate;
    return true;
}
size_t GainJson::encode(const GainProfile &values,const CalibrationData &factory,char *output,size_t capacity) {
    if (!values.valid(factory) || !output || !capacity) return 0;
    auto &document=JsonWorkspace::document<StaticJsonDocument<DocumentBytes>>(); // SRAM4, not the stack.
    document["schema"]=1; document["enabled"]=values.enabled;
    document["factory_crc16"]=values.factoryCrc;
    auto scales=document.createNestedArray("millivolts_per_division");
    for (unsigned r=0;r<GainProfile::Ranges;++r) scales.add(scope::VoltageRanges::millivolts(r));
    auto channels=document.createNestedArray("gain");
    for (const auto &channel:values.gain) {
        auto ranges=channels.createNestedArray();
        for (float factor:channel) ranges.add(factor);
    }
    if (document.overflowed() || measureJson(document)>=capacity) return 0;
    return serializeJson(document,output,capacity);
}
} // namespace openosc::calibration
