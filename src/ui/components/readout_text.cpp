#include "openosc/ui/components/readout_text.hpp"
#include "openosc/ui/components/elements.hpp"
#include "openosc/ui/scope/format.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace openosc::ui {
namespace {
struct Step { unsigned precision; bool coarse; };
constexpr Step Steps[] = {{3, false}, {2, false}, {1, false}, {2, true}, {1, true}};
// The short tags, where a channel's three readouts need the room (Measurements::Tags order).
constexpr const char *ShortTags[] = {"f", "pp", "R", "T", "Av", "Mn", "Mx", "AC", "D", "n", "", "F", "Am", "Tp", "Bt"}; // FC: F, its mark kept ("Ff?").
static_assert(sizeof ShortTags / sizeof *ShortTags == scope::Measurements::Count);
// Form index: its step and whether it takes the short tag.
struct Shape { uint8_t step; bool shortTag; };
// Full tags keep two digits as long as they can (the next prefix: "RMS0.51V" before "R513mV").
constexpr Shape Shapes[ReadoutText::Forms - 1] = {{0, false}, {1, false}, {3, false}, {1, true}, {3, true}, {2, false}, {2, true}, {4, false}, {4, true}};
} // namespace
ReadoutIndicator ReadoutIndicator::of(scope::ReadoutBasis basis, const ReadoutState &state) {
    ReadoutIndicator indicator;
    if (basis != scope::ReadoutBasis::Record) return indicator;
    indicator.full = true;
    indicator.percent = state.running ? uint8_t(state.percent < 99 ? state.percent : 99) : 100; // A running pass is not done.
    indicator.stale = state.older && state.ageMs >= StaleMs;
    return indicator;
}
bool ReadoutText::fit(const char *tag, bool valid, double value, const char *unit, int width, char *output, size_t capacity,
                      const char *suffix) {
    const char *space = *tag ? " " : "";
    for (const auto step : Steps) {
        char formatted[24] = "--";
        if (valid && !plot::Format::quantity(value, unit, formatted, sizeof formatted, step.precision, step.coarse)) continue;
        for (const char *gap : {space, ""}) { // Dropping the space beats losing a digit.
            std::snprintf(output, capacity, "%s%s%s%s", tag, gap, formatted, suffix);
            if (Elements::textWidth(output) <= width) return true;
        }
        if (!valid) break;
    }
    // Nothing fits (a value far outside the instrument's range): say so in the cell, never clip.
    std::snprintf(output, capacity, "%s%sOVR", tag, space);
    return false;
}
unsigned ReadoutText::digits(unsigned index) { return index < Overflow ? Steps[Shapes[index].step].precision : 0; }
bool ReadoutText::form(unsigned kind, bool valid, double value, const char *mark, unsigned index, ReadoutParts &output, unsigned size) {
    output = {};
    output.size = size;
    if (index >= Forms || kind >= scope::Measurements::Count || kind == unsigned(scope::Measurement::Off)) return false;
    const bool overflow = index == Overflow;
    const auto shape = overflow ? Shape{0, false} : Shapes[index];
    if (overflow) { // Only for a value no other form can write (far outside the instrument's range).
        for (unsigned i = 0; i < Overflow; ++i) if (form(kind, valid, value, mark, i, output, size)) return false;
        output = {};
        output.size = size;
        std::snprintf(output.value, sizeof output.value, "OVR");
    } else if (valid) {
        const auto step = Steps[shape.step];
        if (!plot::Format::quantity(value, scope::Measurements::Units[kind], output.value, sizeof output.value, step.precision, step.coarse))
            return false;
        // A percent just under 100 that rounds to "100%" would read as DC: its bound instead, with
        // the decimals the form keeps (">99.9%" at three digits, ">99.99%" at two decimals). A form
        // with no decimal to spare (">99%") says too little: the form is not offered.
        if (std::strstr(output.value, "100") == output.value && std::abs(value) < 100 && !std::strcmp(scope::Measurements::Units[kind], "%")) {
            const unsigned nines = step.coarse ? step.precision : step.precision > 2 ? step.precision - 2 : 0;
            if (!nines) return false;
            char decimals[8] = "";
            for (unsigned i = 0; i < nines && i + 1 < sizeof decimals; ++i) decimals[i] = '9';
            std::snprintf(output.value, sizeof output.value, ">99.%s%%", decimals);
        }
    } else std::snprintf(output.value, sizeof output.value, "--");
    std::snprintf(output.tag, sizeof output.tag, "%s%s", shape.shortTag ? ShortTags[kind] : scope::Measurements::Tags[kind], mark ? mark : "");
    output.tagWidth = Elements::textWidth(output.tag, size);
    output.valueWidth = Elements::textWidth(output.value, size);
    return true;
}
} // namespace openosc::ui
