#include "openosc/settings/instrument_json.hpp"
#include "openosc/settings/arrangement_json.hpp"
#include "openosc/settings/color_value.hpp"
#include "openosc/settings/scope_appearance_json.hpp"
#include <cstring>

namespace openosc {
namespace {
template<class T> bool number(JsonObjectConst object, const char *name, T &value) {
    if (!object.containsKey(name)) return true;
    if (!object[name].is<T>()) return false;
    value = object[name].as<T>();
    return true;
}
bool boolean(JsonObjectConst object, const char *name, uint32_t &value) {
    if (!object.containsKey(name)) return true;
    if (!object[name].is<bool>()) return false;
    value = object[name].as<bool>();
    return true;
}
template<size_t N> bool choice(JsonObjectConst object, const char *name, uint32_t &value,
                               const char *const (&names)[N]) {
    if (!object.containsKey(name)) return true;
    if (!object[name].is<const char *>()) return false;
    for (unsigned i = 0; i < N; ++i)
        if (!std::strcmp(object[name], names[i])) { value = i; return true; }
    return false;
}
constexpr const char *couplings[] = {"dc", "ac"}, *edges[] = {"rising", "falling"};
constexpr const char *triggerModes[] = {"auto", "normal", "single"};
constexpr const char *factoryRecordFormats[] = {"auto", "long", "compact"}; // FactoryRecordPolicy.
constexpr const char *displays[] = {"primary", "statistics", "trend", "analog"};
constexpr const char *faces[] = {"ibm", "mono", "segment"}, *palettes[] = {"dark", "amber", "paper"};
static_assert(sizeof displays / sizeof *displays == MeterPreferences::Displays && sizeof faces / sizeof *faces == MeterPreferences::Faces &&
              sizeof palettes / sizeof *palettes == MeterPreferences::Palettes);
// A colour's file text: a named colour by its static key (not copied), Custom as "#rrggbb" (copied
// into the document), Inherit not written.
void color(JsonObject object, const ColorValue &value) {
    if (value.inherit()) return;
    if (!value.isCustom()) { object["color"] = ColorCatalog::swatch(value.index).key; return; }
    char text[ColorText::Capacity];
    if (ColorText::format(value, text, sizeof text)) object["color"] = static_cast<char *>(text);
}
// meter.appearance: {"<role key>": {"color": "<swatch>|#rrggbb", "font": "<font>", "style": "<style>"}}, only the
// overrides. Each field stands alone: an unknown role, value or type is left out (inherited), never
// the reason to drop the rest of the file (older or newer firmware wrote it).
void appearance(JsonObjectConst object, MeterAppearance &output) {
    for (unsigned i = 0; i < MeterAppearance::Roles; ++i) {
        const auto &info = MeterAppearanceCatalog::role(MeterRole(i));
        const auto role = object[info.key];
        if (!role.is<JsonObjectConst>()) continue;
        auto &style = output.roles[i];
        style.setColor(ColorText::parse(role["color"].as<const char *>())); // A key, "#rrggbb", else Inherit.
        style.font = MeterAppearanceCatalog::find(info.fontKeys, info.fonts, role["font"].as<const char *>());
        style.style = MeterAppearanceCatalog::find(info.styleKeys, info.styles, role["style"].as<const char *>());
    }
}
void appearance(JsonObject meter, const MeterAppearance &values) {
    if (!values.custom()) return; // Files without overrides stay as they were.
    auto object = meter.createNestedObject("appearance");
    for (unsigned i = 0; i < MeterAppearance::Roles; ++i) {
        const auto &style = values.roles[i];
        if (style == MeterRoleStyle{}) continue;
        const auto &info = MeterAppearanceCatalog::role(MeterRole(i));
        auto role = object.createNestedObject(info.key);
        color(role, style.colorValue());
        if (style.font != MeterRoleStyle::Inherit) role["font"] = info.fontKeys[style.font];
        if (style.style != MeterRoleStyle::Inherit) role["style"] = info.styleKeys[style.style];
    }
}
// meter.hold: {"mode": "manual|delayed|stable|auto", "delay_ms", "stable_ms", "band_digits", "rearm_digits",
// "min_readings"}. Each value stands alone against HoldSettings::valid (the editor's and SCPI's check):
// one out of range or of another type keeps its default, never its neighbours or the rest of the file.
// The band and the rearm threshold are one pair (rearm > band): taken together when they fit, else
// each alone where it still fits the other.
void hold(JsonObjectConst object, dmm::HoldSettings &output) {
    dmm::HoldSettings value;
    auto take = [&](const char *name, uint32_t dmm::HoldSettings::*field) {
        if (!object[name].is<uint32_t>()) return;
        auto next = value;
        next.*field = object[name].as<uint32_t>();
        if (next.valid()) value = next;
    };
    dmm::HoldMode mode;
    if (dmm::HoldSettings::fromKey(object["mode"].as<const char *>(), mode)) value.mode = mode;
    take("delay_ms", &dmm::HoldSettings::delayMs);
    take("stable_ms", &dmm::HoldSettings::stableMs);
    take("min_readings", &dmm::HoldSettings::minReadings);
    if (object["band_digits"].is<uint32_t>() && object["rearm_digits"].is<uint32_t>()) {
        auto pair = value;
        pair.bandDigits = object["band_digits"].as<uint32_t>();
        pair.rearmDigits = object["rearm_digits"].as<uint32_t>();
        if (pair.valid()) value = pair;
    }
    take("band_digits", &dmm::HoldSettings::bandDigits);
    take("rearm_digits", &dmm::HoldSettings::rearmDigits);
    output = value;
}
void hold(JsonObject meter, const dmm::HoldSettings &values) {
    auto object = meter.createNestedObject("hold");
    object["mode"] = dmm::HoldSettings::key(values.mode);
    object["delay_ms"] = values.delayMs; object["stable_ms"] = values.stableMs;
    object["band_digits"] = values.bandDigits; object["rearm_digits"] = values.rearmDigits;
    object["min_readings"] = values.minReadings;
}
}
bool InstrumentJson::parse(JsonObjectConst scope, JsonObjectConst meter, Preferences &output) {
    auto &s = output.scope;
    // "record_points" is the total of the enabled channels. Files before it stored "samples", a
    // depth per channel (4..65536); it migrates to the smallest budget keeping at least that depth.
    uint32_t legacySamples = 0;
    const bool budget = scope.containsKey("record_points"), legacy = !budget && scope.containsKey("samples");
    if ((budget && !number(scope, "record_points", s.recordPoints)) || (legacy && !number(scope, "samples", legacySamples)) ||
        (legacy && (legacySamples < 4 || legacySamples > 65536 || (legacySamples & 3)))) return false;
    if (!(scope.containsKey("run_mode") ? number(scope,"run_mode",s.runMode) : boolean(scope,"high_performance",s.runMode)) || !boolean(scope,"roll_auto",s.rollAuto) ||
        !choice(scope, "factory_record_format", s.factoryRecordFormat, factoryRecordFormats) ||
        !choice(meter, "display", output.meter.display, displays) ||
        !boolean(meter, "statistics", output.meter.statistics) || !boolean(meter,"auto",output.meter.automatic) ||
        !number(meter,"range_index",output.meter.range) || !choice(meter, "face", output.meter.face, faces) ||
        !choice(meter, "palette", output.meter.palette, palettes)) return false;
    if (meter["appearance"].is<JsonObjectConst>()) appearance(meter["appearance"].as<JsonObjectConst>(), output.meter.appearance);
    if (meter["hold"].is<JsonObjectConst>()) hold(meter["hold"].as<JsonObjectConst>(), output.meter.hold);
    arrangement(meter, "arrangement_digital", output.meter.digitalArrangement);
    arrangement(meter, "arrangement_analog", output.meter.analogArrangement);
    arrangement(scope, "arrangement", s.arrangement);
    if (scope["appearance"].is<JsonObjectConst>()) ScopeAppearanceJson::parse(scope["appearance"].as<JsonObjectConst>(), s.appearance);
    if (meter.containsKey("range_function")) { // The owner of range_index; absent: unknown.
        if (!meter["range_function"].is<const char *>()) return false;
        unsigned f = 0;
        while (f < MeterFunctionCount && std::strcmp(meter["range_function"], MeterFunctions[f].key)) ++f;
        if (f == MeterFunctionCount) return false;
        output.meter.rangeFunction = f;
    }
    if (scope.containsKey("channels")) {
        const auto channels = scope["channels"].as<JsonArrayConst>();
        if (channels.size() != 2) return false;
        for (unsigned c = 0; c < 2; ++c) {
            if (!channels[c].is<JsonObjectConst>()) return false;
            const auto channel = channels[c].as<JsonObjectConst>();
            if (!boolean(channel, "enabled", s.enabled[c]) || !choice(channel, "coupling", s.coupling[c], couplings) ||
                !number(channel, "position", s.position[c]) || !number(channel, "probe_numerator", s.probeNumerator[c]) ||
                !number(channel, "probe_denominator", s.probeDenominator[c])) return false;
        }
    }
    if (scope.containsKey("trigger")) {
        if (!scope["trigger"].is<JsonObjectConst>()) return false;
        const auto trigger = scope["trigger"].as<JsonObjectConst>();
        if (!number(trigger, "source", s.triggerSource) || !number(trigger, "level_codes", s.triggerLevel) ||
            !number(trigger,"position_permille",s.triggerPosition) || !boolean(trigger,"noise_reject",s.triggerReject) ||
            !choice(trigger, "edge", s.triggerEdge, edges) || !choice(trigger, "mode", s.triggerMode, triggerModes)) return false;
    }
    if (scope.containsKey("measurements")) {
        const auto measurements = scope["measurements"].as<JsonArrayConst>();
        if (measurements.size()!=2) return false;
        if (measurements[0].is<uint32_t>() && measurements[1].is<uint32_t>()) {
            for (auto &channel : s.measurements.values) {
                channel[0]=measurements[0]; channel[1]=measurements[1];
                channel[2]=channel[3]=unsigned(scope::Measurement::Off);
            }
        } else for (unsigned c=0; c<2; ++c) {
            const auto channel=measurements[c].as<JsonArrayConst>();
            if (channel.size()!=scope::ReadoutSelection::PerChannel) return false;
            for (unsigned i=0; i<scope::ReadoutSelection::PerChannel; ++i) {
                if (!channel[i].is<uint32_t>()) return false;
                s.measurements.values[c][i]=channel[i];
            }
        }
    }
    if (scope.containsKey("measure_basis")) {
        const char *basis=scope["measure_basis"] | "";
        if (!std::strcmp(basis,"screen")) s.measurements.basis=scope::ReadoutBasis::Screen;
        else if (!std::strcmp(basis,"record")) s.measurements.basis=scope::ReadoutBasis::Record;
        else return false;
    }
    if (legacy) s.recordPoints = scope::RecordBudget::covering(legacySamples, scope::RecordBudget::channels(s.enabled[0], s.enabled[1]));
    return s.valid() && output.meter.valid();
}
void InstrumentJson::encode(JsonObject scope, JsonObject meter, const Preferences &values) {
    const auto &s = values.scope;
    scope["record_points"] = s.recordPoints;
    scope["run_mode"] = s.runMode;
    scope["roll_auto"]=bool(s.rollAuto);
    if (s.factoryRecordFormat) scope["factory_record_format"] = factoryRecordFormats[s.factoryRecordFormat]; // Absent: auto.
    auto channels = scope.createNestedArray("channels");
    for (unsigned c = 0; c < 2; ++c) {
        auto channel = channels.createNestedObject();
        channel["enabled"] = bool(s.enabled[c]); channel["coupling"] = couplings[s.coupling[c]];
        channel["position"] = s.position[c]; channel["probe_numerator"] = s.probeNumerator[c];
        channel["probe_denominator"] = s.probeDenominator[c];
    }
    auto trigger = scope.createNestedObject("trigger");
    trigger["source"] = s.triggerSource; trigger["edge"] = edges[s.triggerEdge];
    trigger["mode"] = triggerModes[s.triggerMode]; trigger["level_codes"] = s.triggerLevel;
    trigger["position_permille"]=s.triggerPosition;
    trigger["noise_reject"]=bool(s.triggerReject);
    auto measurements = scope.createNestedArray("measurements");
    for (const auto &channel : s.measurements.values) {
        auto row=measurements.createNestedArray();
        for (auto value : channel) row.add(value);
    }
    scope["measure_basis"] = s.measurements.basis==scope::ReadoutBasis::Record ? "record" : "screen";
    arrangement(scope, "arrangement", s.arrangement);
    ScopeAppearanceJson::encode(scope, s.appearance);
    meter["display"] = displays[values.meter.display]; meter["statistics"] = bool(values.meter.statistics);
    meter["auto"] = bool(values.meter.automatic); meter["range_index"] = values.meter.range;
    meter["face"] = faces[values.meter.face]; meter["palette"] = palettes[values.meter.palette];
    arrangement(meter, "arrangement_digital", values.meter.digitalArrangement);
    arrangement(meter, "arrangement_analog", values.meter.analogArrangement);
    if (values.meter.rangeFunction < MeterFunctionCount) meter["range_function"] = MeterFunctions[values.meter.rangeFunction].key;
    appearance(meter, values.meter.appearance);
    hold(meter, values.meter.hold);
}
} // namespace openosc
