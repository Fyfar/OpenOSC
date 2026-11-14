#include "openosc/calibration/interleave_file.hpp"
#include "openosc/settings/json_input.hpp"
#include <ArduinoJson.h>

namespace openosc::calibration {
bool InterleaveJson::parse(const char *text,size_t bytes,const CalibrationData &factory,InterleaveProfile &output) {
    if (bytes>MaxBytes || !JsonInput::completeObject(text,bytes,2)) return false;
    StaticJsonDocument<JSON_OBJECT_SIZE(4)+64> document;
    if (deserializeJson(document,text,bytes,DeserializationOption::NestingLimit(2))) return false;
    const auto root=document.as<JsonObjectConst>();
    if (!root["schema"].is<unsigned>() || root["schema"].as<unsigned>()!=1 || !root["enabled"].is<bool>() ||
        !root["factory_crc16"].is<uint16_t>() || root["factory_crc16"].as<uint16_t>()!=factory.crc() ||
        !root["offset_codes"].is<float>()) return false;
    InterleaveProfile candidate; candidate.factoryCrc=factory.crc();
    candidate.enabled=root["enabled"]; candidate.offset=root["offset_codes"].as<float>();
    if (!candidate.valid(factory)) return false;
    output=candidate;
    return true;
}
size_t InterleaveJson::encode(const InterleaveProfile &values,const CalibrationData &factory,char *output,size_t capacity) {
    if (!values.valid(factory) || !output || !capacity) return 0;
    StaticJsonDocument<JSON_OBJECT_SIZE(4)+64> document;
    document["schema"]=1; document["enabled"]=values.enabled;
    document["factory_crc16"]=values.factoryCrc; document["offset_codes"]=values.offset;
    if (document.overflowed() || measureJson(document)>=capacity) return 0;
    return serializeJson(document,output,capacity);
}
} // namespace openosc::calibration
