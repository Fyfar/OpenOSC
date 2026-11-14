#pragma once
namespace openosc {
class PowerControl {
public:
    static void backlight(bool enabled);
    static void brightness(unsigned percent);
    // After a core clock change: the backlight PWM back at 1 kHz from the new APB2 timer clock.
    static void retime();
    [[noreturn]] static void powerOff();
};
}
