#include "openosc/application/controller.hpp"
#include "openosc/generator/capabilities.hpp"
#include "openosc/generator/frequency.hpp"
#include "openosc/platform/input.hpp"
#include <algorithm>

namespace openosc {
void ApplicationController::generatorKeys(uint32_t keys, uint32_t now) {
    using Caps = generator::Capabilities;
    auto &output = data_.generator();
    if (keys & (Keypad::F1 | Keypad::F3)) {
        const bool wave = keys & Keypad::F1;
        view_.focusParameter(wave ? 0 : 2);
        generatorEditor_.begin(output.settings(), wave);
        openPanel(Panel::Generator, generatorEditor_); return;
    }
    if (keys & (Keypad::F4 | Keypad::HoldSave)) view_.focusParameter(3);
    if (keys & (Keypad::F2 | Keypad::Menu)) {
        frequencyEditor_.begin(prefs_.generatorMillihertz, Caps::maximumMillihertz(output.settings().shape, output.settings().level),
                               Caps::MinimumMillihertz);
        panel_ = Panel::Frequency;
        view_.frequencyEditor(frequencyEditor_);
        return;
    }
    if (keys & (Keypad::HoldSave | Keypad::F4)) {
        if (output.enabled()) output.disable();
        else if (!output.enable()) view_.notify("Cannot enable output", now);
    }
    auto settings = output.settings();
    uint64_t frequency = prefs_.generatorMillihertz;
    const uint64_t step = std::max<uint64_t>(1, frequency / 10);
    const uint64_t maximum = Caps::maximumMillihertz(settings.shape, settings.level);
    if (keys & Keypad::Right) frequency = std::min(maximum, frequency + step);
    if (keys & Keypad::Left) frequency = std::max(Caps::MinimumMillihertz, frequency > step ? frequency - step : 1);
    settings.frequencyHz = generator::Frequency::rational(frequency);
    if (Caps::duty(settings.shape)) {
        const int duty = int(settings.dutyPermille) + ((keys & Keypad::Up) ? 50 : (keys & Keypad::Down) ? -50 : 0);
        settings.dutyPermille = uint16_t(std::clamp<int>(duty, Caps::MinimumDutyPermille, Caps::MaximumDutyPermille));
    } else if (keys & (Keypad::Up | Keypad::Down)) {
        if (Caps::Levels) {
            settings.level = generator::Level((unsigned(settings.level) + ((keys & Keypad::Up) ? 1 : 3)) % 4);
            settings.frequencyHz = generator::Frequency::rational(
                std::min(frequency, Caps::maximumMillihertz(settings.shape, settings.level)));
        } else { // The module's amplitude, 0.1 V steps up to the shape's limit at this frequency.
            const int top = Caps::amplitudeLimitMillivolts(settings.shape, frequency);
            const int next = int(settings.amplitudeMillivolts) + ((keys & Keypad::Up) ? 1 : -1) * Caps::AmplitudeStepMillivolts;
            settings.amplitudeMillivolts = uint16_t(std::clamp(next, 0, top));
        }
    }
    if (Caps::Amplitude) settings.amplitudeMillivolts = std::min(settings.amplitudeMillivolts, Caps::amplitudeLimitMillivolts(settings.shape, frequency));
    if (keys & (Keypad::Left | Keypad::Right | Keypad::Up | Keypad::Down)) {
        if (!applyGenerator(settings, now)) view_.notify("Unsupported output settings", now);
    }
}
} // namespace openosc
