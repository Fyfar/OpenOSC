#include "openosc/measurement/dmm/diagnostic_format.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::dmm {
namespace {
bool terminated(const char *text,size_t bytes) { return std::memchr(text,0,bytes)!=nullptr; }
}
size_t DiagnosticFormat::format(const Result &result,char *output,size_t capacity) {
    if (!output || !capacity) return 0;
    output[0]=0;
    if (!terminated(result.mainText,sizeof result.mainText) || !result.mainText[0] ||
        !terminated(result.rawUnit,sizeof result.rawUnit) || !terminated(result.unit,sizeof result.unit)) return 0;
    const char *unit=result.rawUnit[0] ? result.rawUnit : result.unit;
    const char *kind=nullptr;
    switch (result.kind) {
    case Kind::Dc: case Kind::Ac:
        kind=unit[0]=='V' || (unit[0]=='m' && unit[1]=='V') ? "Voltage" : "Electricity"; break;
    case Kind::Continuity: kind="beep"; break;
    case Kind::Capacitance: kind="Cap"; break;
    case Kind::Diode: kind="Diode"; break;
    case Kind::Resistance: kind="Resistance"; break;
    // Kind7 (DiodeWithBeeper): the factory formatter assigns no label of its own and keeps the
    // previous one; this one writes no line rather than reuse a label from a previous function.
    default: return 0;
    }
    if (!std::strcmp(unit,"&") || !std::strcmp(unit,"Ohm")) unit="OM";
    else if (!std::strcmp(unit,"K&") || !std::strcmp(unit,"kOhm")) unit="KOM";
    else if (!std::strcmp(unit,"M&") || !std::strcmp(unit,"MOhm")) unit="MOM";
    const int bytes=std::snprintf(output,capacity,"%s:%s %s",kind,Decoder::recordText(result),unit); // "?" for a non-value that reads as one.
    if (bytes<0 || size_t(bytes)>=capacity) { output[0]=0; return 0; }
    return size_t(bytes);
}
} // namespace openosc::dmm
