#include "openosc/measurement/dmm/ranges.hpp"
#include <cmath>
#include <cstring>

namespace openosc::dmm {
namespace {
// Nominal full scales from the manufacturer manuals' multimeter specifications: ZT-703S pp. 32-34
// (25000 counts), ZT-706S pp. 32-34 (66000 counts; its 6.6000 V row belongs to AC V, the row that
// AC V's accuracy is centred on, as DC V's). They describe the reported range; they are not
// calibration gains. Neither manual lists a uA range, nor ranges for diode and continuity.
struct Table { const RangeInfo *values; unsigned count; };
template<size_t N> constexpr Table table(const RangeInfo (&values)[N]) { return {values, N}; }
constexpr RangeInfo capacitance[] = {{"nF",3,9.999e-9},{"nF",2,99.99e-9},{"nF",1,999.9e-9}, // Both manuals.
    {"uF",3,9.999e-6},{"uF",2,99.99e-6},{"uF",1,999.9e-6},{"mF",3,9.999e-3},{"mF",2,99.99e-3}};
namespace zt703s {
constexpr RangeInfo voltsDc[] = {{"V",4,2.5},{"V",3,25},{"V",2,250},{"V",1,1000}};
constexpr RangeInfo voltsAc[] = {{"V",4,2.5},{"V",3,25},{"V",2,250},{"V",1,750}};
constexpr RangeInfo millivolts[] = {{"mV",3,.025},{"mV",2,.25}};
constexpr RangeInfo amps[] = {{"A",4,2.5},{"A",3,10}};
constexpr RangeInfo milliamps[] = {{"mA",3,.025},{"mA",2,.25}};
constexpr RangeInfo resistance[] = {{"Ohm",2,250},{"kOhm",4,2500},{"kOhm",3,25000},{"kOhm",2,250000},
    {"MOhm",4,2500000},{"MOhm",3,25000000},{"MOhm",1,250000000}};
}
namespace zt706s {
constexpr RangeInfo voltsDc[] = {{"V",4,6.6},{"V",3,66},{"V",2,660},{"V",1,1000}};
constexpr RangeInfo voltsAc[] = {{"V",4,6.6},{"V",3,66},{"V",2,660},{"V",1,750}};
constexpr RangeInfo millivolts[] = {{"mV",3,.066},{"mV",2,.66}};
constexpr RangeInfo amps[] = {{"A",4,6.6},{"A",3,10}};
constexpr RangeInfo milliamps[] = {{"mA",3,.066},{"mA",2,.66}};
constexpr RangeInfo resistance[] = {{"Ohm",2,660},{"kOhm",4,6600},{"kOhm",3,66000},{"kOhm",2,660000},
    {"MOhm",4,6600000},{"MOhm",3,66000000}};
}
// An overload display that does not carry its range's decimals: the controller's own OL packet,
// captured on the instrument, recognised as that range only while it is an overload.
struct OverloadAlias { Function function; const char *unit; unsigned decimals, index; };
// ZT703S: in the 25.000 MOhm range (index 5) the OL packet places the point as two decimals
// (ff001061eb00000000000000feffff0e50ba; HIL 2026-11-15, build/reviews/quality-completion/
// dmm-range-keys-entry.json). The other six resistance ranges' OL packets carry their own decimals.
// No numeric reading in that range has been captured, so a non-OL MOhm/2 stays unrecognised.
constexpr OverloadAlias zt703sOverload[] = {{Function::Resistance, "MOhm", 2, 5}};
struct Model {
    Table voltsDc, voltsAc, millivolts, amps, milliamps, resistance;
    const OverloadAlias *overload; unsigned overloads;
};
constexpr Model Zt703s{table(zt703s::voltsDc), table(zt703s::voltsAc), table(zt703s::millivolts),
                       table(zt703s::amps), table(zt703s::milliamps), table(zt703s::resistance),
                       zt703sOverload, sizeof zt703sOverload / sizeof *zt703sOverload};
constexpr Model Zt706s{table(zt706s::voltsDc), table(zt706s::voltsAc), table(zt706s::millivolts),
                       table(zt706s::amps), table(zt706s::milliamps), table(zt706s::resistance), nullptr, 0};
const Model &model(board::DmmCounts counts) { return counts == board::DmmCounts::Counts66000 ? Zt706s : Zt703s; }
Table of(const Model &m, Function f) {
    switch (f) {
    case Function::VoltageDc: return m.voltsDc;
    case Function::VoltageAc: return m.voltsAc;
    case Function::MillivoltsDc: case Function::MillivoltsAc: return m.millivolts;
    case Function::CurrentDc: case Function::CurrentAc: return m.amps;
    case Function::MilliampsDc: case Function::MilliampsAc: return m.milliamps;
    case Function::Resistance: return m.resistance;
    case Function::Capacitance: return table(capacitance);
    default: return {nullptr, 0};
    }
}
}
const RangeInfo *Ranges::list(Function f, board::DmmCounts counts, unsigned &count) {
    const auto t = of(model(counts), f);
    count = t.count; return t.values;
}
const RangeInfo *Ranges::list(Function f, unsigned &count) { return list(f, board::Current::Features.dmmCounts, count); }
const RangeInfo *Ranges::current(const Result &result, unsigned *index) {
    unsigned count; const auto *ranges = list(Functions::identify(result), count);
    unsigned decimals = result.decimals;
    if (decimals == 255 && result.primary.valid()) {
        const char *dot = std::strchr(result.mainText, '.');
        if (dot) decimals = std::strlen(dot + 1);
    }
    for (unsigned i = 0; i < count; ++i)
        if (ranges[i].decimals == decimals && !std::strcmp(result.unit, ranges[i].unit)) {
            if (index) *index = i;
            return ranges + i;
        }
    if (result.primary.status != DataStatus::Overload) return nullptr;
    const auto &m = model(board::Current::Features.dmmCounts);
    for (unsigned i = 0; i < m.overloads; ++i) {
        const auto &alias = m.overload[i];
        if (alias.function == Functions::identify(result) && alias.decimals == decimals && alias.index < count &&
            !std::strcmp(result.unit, alias.unit)) {
            if (index) *index = alias.index;
            return ranges + alias.index;
        }
    }
    return nullptr;
}
bool Ranges::choose(Function function, double maximumSi, unsigned &index) {
    if (!std::isfinite(maximumSi) || maximumSi <= 0) return false;
    unsigned count; const auto *ranges = list(function, count);
    for (unsigned i = 0; i < count; ++i) if (ranges[i].maximumSi >= maximumSi) { index = i; return true; }
    return false;
}
bool RangeSelector::request(unsigned index, const Result &latest, uint32_t now) {
    if (busy()) return index == target_;
    function_ = Functions::identify(latest);
    Ranges::list(function_, maximum_);
    if (index >= maximum_ || latest.peerRelative || latest.range == RangeMode::Unknown) return false;
    unsigned actual = 0;
    manual_ = latest.range == RangeMode::Manual && Ranges::current(latest, &actual);
    target_ = index; attempts_ = 0; waiting_ = false; index_ = actual;
    deadline_ = now + DeadlineMs;
    status_ = manual_ && actual == index ? DataStatus::Ok : DataStatus::Busy;
    return true;
}
bool RangeSelector::poll(const Result &latest, uint32_t now, CommandGroup &command) {
    if (!busy()) return false;
    if (Functions::identify(latest) != function_ || latest.peerRelative) { status_ = DataStatus::InvalidArgument; return false; }
    if (int32_t(now - deadline_) >= 0) { status_ = DataStatus::Timeout; return false; }
    const bool fresh = !waiting_ || (latest.sequence != sequence_ && int32_t(now - (sentAt_ + SettleMs)) >= 0);
    unsigned actual = 0;
    const bool manual = latest.range == RangeMode::Manual && Ranges::current(latest, &actual);
    if (fresh && manual && actual == target_) { status_ = DataStatus::Ok; return false; }
    if (waiting_) {
        const uint32_t since = now - sentAt_;
        if (!manual_) { // The first press from Auto: MANU may come late; never a second press before it.
            if (fresh && manual) { manual_ = true; index_ = actual; }
            else { if (since >= ManualMs) status_ = DataStatus::Unsupported; return false; }
        } else if (!fresh || !manual || actual == index_) { // A step not yet shown (or Manual left).
            if (since >= StepMs) status_ = DataStatus::Timeout;
            return false;
        }
        index_ = actual;
    }
    if (attempts_ >= maximum_) { status_ = DataStatus::Timeout; return false; } // N ranges: N presses at most.
    waiting_ = false; command = CommandGroup::NextRange; return true;
}
} // namespace openosc::dmm
