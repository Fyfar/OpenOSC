#pragma once
#include "openosc/application/state.hpp"
#include "openosc/ui/screens/screen_host.hpp"

namespace openosc::ui {
class InstrumentScreen {
public:
    explicit InstrumentScreen(ScreenHost &host) : host_(host) {}
    virtual ~InstrumentScreen() = default;
    virtual void activate(const ApplicationState &state) = 0;

protected:
    ScreenHost &host_;
};
} // namespace openosc::ui
