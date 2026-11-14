#pragma once
#include "openosc/measurement/dmm/function.hpp"
#include "openosc/ui/screens/instrument_screen.hpp"
#include "openosc/ui/screens/meter_context.hpp"
#include "openosc/ui/components/meter_style.hpp"

namespace openosc::ui {
// The DMM screen: places the function sign, the reading or the analog dial, the summary/trend and
// applies the DMM palette and reading face. The components draw; this only arranges and feeds them.
class MeterScreen final : public InstrumentScreen {
public:
    static constexpr unsigned AnalogDisplay = 3; // MeterPreferences::display.
    // The analog layout's rows (layout 2): a 12 px meta row above the dial's tick numbers, the dial
    // with its overload peg and needle, MIN/MAX/AVG, then the 64 px reading with its unit.
    struct AnalogLayout {
        static constexpr int MetaY = 55, DialY = 64, DialHeight = 92, StatsY = 158, ReadingY = 177, ReadingHeight = 62, UnitGap = 8;
        static_assert(DialY + DialHeight <= StatsY && StatsY + 14 < ReadingY && ReadingY + ReadingHeight <= 240);
    };
    using InstrumentScreen::InstrumentScreen;
    void activate(const ApplicationState &state) override;
    void update(const dmm::Result &result, bool held, DataStatus status, const MeterContext &context);
    void status(const dmm::Result &result, bool held, DataStatus status, bool relative = false);
    // The bar and the needle, every ~8 ms: `now` is the monotonic time the needle moves by.
    void fastBar(const dmm::Result &result, DataStatus status, const dmm::RelativeReading *relative, uint32_t now);

    const MeterLook &look() const { return look_; } // What is applied (tests).

private:
    void feed(const dmm::Result &result, DataStatus status, const dmm::RelativeReading *relative); // Bar or dial target.
    void reading(const char *text); // The main reading, its sign column reserved (MeterReading).
    // The DMM's look (palette, face, overrides): resolved and applied only when it changes.
    void style(unsigned face, unsigned palette, const MeterAppearance &appearance);
    void layout(bool analog);
    void cards(dmm::Function function, bool beeper, bool currentInput, unsigned focus, const char *unit);
    unsigned face_ = 0, palette_ = 0;
    MeterAppearance appearance_;
    MeterLook look_;
    bool analog_ = false, held_ = false, styled_ = false;
};
} // namespace openosc::ui
