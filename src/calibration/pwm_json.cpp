#include "openosc/calibration/pwm_json.hpp"
#include "openosc/settings/json_input.hpp"
#include "openosc/acquisition/ranges.hpp"
#include "openosc/settings/json_workspace.hpp"
#include <ArduinoJson.h>

namespace openosc::calibration {
namespace {
constexpr size_t DocumentBytes=JSON_OBJECT_SIZE(6)+JSON_ARRAY_SIZE(2)+2*JSON_ARRAY_SIZE(11)+
    22*JSON_ARRAY_SIZE(7)+JSON_ARRAY_SIZE(7)+JSON_ARRAY_SIZE(11)+256;
}
bool PwmJson::parse(const char *text,size_t bytes,const CalibrationData &factory,PwmProfile &output) {
    if (bytes>MaxBytes || !JsonInput::completeObject(text,bytes,5)) return false;
    JsonWorkspace::Document<DocumentBytes> lease; // Off the stack (JsonWorkspace).
    if (!lease) return false;
    auto &document=*lease;
    if (deserializeJson(document,text,bytes,DeserializationOption::NestingLimit(5))) return false;
    const auto root=document.as<JsonObjectConst>();
    if (!root["schema"].is<unsigned>() || root["schema"].as<unsigned>()!=1 ||
        !root["enabled"].is<bool>() || !root["factory_crc16"].is<uint16_t>() ||
        root["factory_crc16"].as<uint16_t>()!=factory.crc()) return false;
    const auto channels=root["compare"].as<JsonArrayConst>();
    const auto positions=root["positions"].as<JsonArrayConst>();
    const auto scales=root["millivolts_per_division"].as<JsonArrayConst>();
    if (channels.size()!=PwmProfile::Channels || positions.size()!=PwmProfile::Points || scales.size()!=PwmProfile::Ranges) return false;
    for (unsigned p=0;p<PwmProfile::Points;++p)
        if (!positions[p].is<int>() || positions[p].as<int>()!=ZeroProfile::Positions[p]) return false;
    for (unsigned r=0;r<PwmProfile::Ranges;++r)
        if (!scales[r].is<unsigned>() || scales[r].as<unsigned>()!=scope::VoltageRanges::millivolts(r)) return false;
    PwmProfile candidate; candidate.factoryCrc=factory.crc(); candidate.enabled=root["enabled"];
    for (unsigned c=0;c<PwmProfile::Channels;++c) {
        const auto ranges=channels[c].as<JsonArrayConst>();
        if (ranges.size()!=PwmProfile::Ranges) return false;
        for (unsigned r=0;r<PwmProfile::Ranges;++r) {
            const auto points=ranges[r].as<JsonArrayConst>();
            if (points.size()!=PwmProfile::Points) return false;
            for (unsigned p=0;p<PwmProfile::Points;++p) {
                if (!points[p].is<uint16_t>()) return false;
                candidate.compare[c][r][p]=points[p];
            }
        }
    }
    if (!candidate.valid(factory)) return false;
    output=candidate;
    return true;
}
size_t PwmJson::encode(const PwmProfile &values,const CalibrationData &factory,char *output,size_t capacity) {
    if (!values.valid(factory) || !output || !capacity) return 0;
    JsonWorkspace::Document<DocumentBytes> lease; // Off the stack (JsonWorkspace).
    if (!lease) return 0;
    auto &document=*lease;
    document["schema"]=1; document["enabled"]=values.enabled;
    document["factory_crc16"]=values.factoryCrc;
    auto positions=document.createNestedArray("positions");
    for (int position:ZeroProfile::Positions) positions.add(position);
    auto scales=document.createNestedArray("millivolts_per_division");
    for (unsigned r=0;r<PwmProfile::Ranges;++r) scales.add(scope::VoltageRanges::millivolts(r));
    auto channels=document.createNestedArray("compare");
    for (const auto &channel:values.compare) {
        auto ranges=channels.createNestedArray();
        for (const auto &range:channel) {
            auto points=ranges.createNestedArray();
            for (uint16_t compare:range) points.add(compare);
        }
    }
    if (document.overflowed() || measureJson(document)>=capacity) return 0;
    return serializeJson(document,output,capacity);
}
} // namespace openosc::calibration
