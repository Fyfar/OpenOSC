#pragma once
#include "openosc/ui/screens/instrument_screen.hpp"
#include "openosc/ui/components/soft_keys.hpp"

namespace openosc::ui {
// What F1-F4 do on the generator screen now: its own keys (Main: Wave, Frequency, Params, Output), or
// a panel's (Wave, Params, Frequency): F1 Wave, F2 Frequency (in Frequency: Unit, its Hz/kHz/MHz),
// F3 Params, F4 Back in a panel's list or Cancel while a field or the frequency is edited.
struct GeneratorKeys {
    enum class Panel : uint8_t { Main, Wave, Params, Frequency };
    Panel panel = Panel::Main;
    bool editing = false;      // A field in progress (Choice or Number): F4 cancels it.
    const char *unit = nullptr; // Frequency: the unit F2 steps from.
};
class GeneratorScreen final : public InstrumentScreen {
public:
    using InstrumentScreen::InstrumentScreen;
    // Back to the screen's own keys, with the last output shown as it is (not placeholders).
    void activate(const ApplicationState &state) override;
    void update(const generator::Snapshot &snapshot);
    void status(const generator::Snapshot &snapshot);
    // A panel's keys (ParameterBar::context) with the last output's values; the texts live here.
    const SoftKeys &keys(const GeneratorKeys &keys);

private:
    void values(); // The cards' values from the last output: shape, frequency, duty or level.
    void cards();  // The screen's own four cards (Main).
    generator::Snapshot snapshot_;
    SoftKeys keys_;
    char shape_[16] = "--", frequency_[32] = "--", params_[24] = "Fixed", unit_[8] = "";
    bool shown_ = false; // snapshot_ holds an update: activate() shows it rather than placeholders.
};
} // namespace openosc::ui
