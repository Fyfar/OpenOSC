#include "openosc/calibration/zero_json.hpp"
#include "openosc/settings/json_input.hpp"
#include "openosc/settings/json_workspace.hpp"
#include "openosc/acquisition/ranges.hpp"
#include <ArduinoJson.h>

namespace openosc::calibration {
namespace {
constexpr size_t DocumentBytes=JSON_OBJECT_SIZE(6)+JSON_ARRAY_SIZE(2)+2*JSON_ARRAY_SIZE(11)+
    22*JSON_ARRAY_SIZE(7)+JSON_ARRAY_SIZE(7)+JSON_ARRAY_SIZE(11)+256;
}
bool ZeroJson::parse(const char *text,size_t bytes,uint16_t factoryCrc,ZeroProfile &output) {
    if (bytes>MaxBytes || !JsonInput::completeObject(text,bytes,5)) return false;
    auto &document=JsonWorkspace::document<StaticJsonDocument<DocumentBytes>>(); // SRAM4, not the stack.
    if (deserializeJson(document,text,bytes,DeserializationOption::NestingLimit(5))) return false;
    const auto root=document.as<JsonObjectConst>();
    if (!root["schema"].is<unsigned>() || root["schema"].as<unsigned>()!=1 ||
        !root["enabled"].is<bool>() || !root["factory_crc16"].is<uint16_t>() ||
        root["factory_crc16"].as<uint16_t>()!=factoryCrc) return false;
    const auto channels=root["zero_codes"].as<JsonArrayConst>();
    if (channels.size()!=ZeroProfile::Channels) return false;
    const auto positions=root["positions"].as<JsonArrayConst>();
    const auto scales=root["millivolts_per_division"].as<JsonArrayConst>();
    if (positions.size()!=ZeroProfile::Points || scales.size()!=ZeroProfile::Ranges) return false;
    for (unsigned p=0;p<ZeroProfile::Points;++p)
        if (!positions[p].is<int>() || positions[p].as<int>()!=ZeroProfile::Positions[p]) return false;
    for (unsigned r=0;r<ZeroProfile::Ranges;++r)
        if (!scales[r].is<unsigned>() || scales[r].as<unsigned>()!=scope::VoltageRanges::millivolts(r)) return false;
    ZeroProfile candidate; candidate.factoryCrc=factoryCrc; candidate.enabled=root["enabled"];
    for (unsigned c=0;c<ZeroProfile::Channels;++c) {
        const auto ranges=channels[c].as<JsonArrayConst>();
        if (ranges.size()!=ZeroProfile::Ranges) return false;
        for (unsigned r=0;r<ZeroProfile::Ranges;++r) {
            const auto points=ranges[r].as<JsonArrayConst>();
            if (points.size()!=ZeroProfile::Points) return false;
            for (unsigned p=0;p<ZeroProfile::Points;++p) {
                if (!points[p].is<float>()) return false;
                candidate.codes[c][r][p]=points[p];
            }
        }
    }
    if (!candidate.valid()) return false;
    output=candidate;
    return true;
}
size_t ZeroJson::encode(const ZeroProfile &values,char *output,size_t capacity) {
    if (!values.valid() || !output || !capacity) return 0;
    auto &document=JsonWorkspace::document<StaticJsonDocument<DocumentBytes>>(); // SRAM4, not the stack.
    document["schema"]=1; document["enabled"]=values.enabled;
    document["factory_crc16"]=values.factoryCrc;
    auto positions=document.createNestedArray("positions");
    for (int position:ZeroProfile::Positions) positions.add(position);
    auto scales=document.createNestedArray("millivolts_per_division");
    for (unsigned r=0;r<ZeroProfile::Ranges;++r) scales.add(scope::VoltageRanges::millivolts(r));
    auto channels=document.createNestedArray("zero_codes");
    for (const auto &channel:values.codes) {
        auto ranges=channels.createNestedArray();
        for (const auto &range:channel) {
            auto points=ranges.createNestedArray();
            for (float code:range) points.add(code);
        }
    }
    if (document.overflowed() || measureJson(document)>=capacity) return 0;
    return serializeJson(document,output,capacity);
}
} // namespace openosc::calibration
