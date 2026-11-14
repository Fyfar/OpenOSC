#pragma once
#include "openosc/ui/screens/instrument_screen.hpp"

namespace openosc::ui {
class GeneratorScreen final : public InstrumentScreen {
public:
    using InstrumentScreen::InstrumentScreen;
    void activate(const ApplicationState &state) override;
    void update(const generator::Snapshot &snapshot);
    void status(const generator::Snapshot &snapshot);
};
} // namespace openosc::ui
