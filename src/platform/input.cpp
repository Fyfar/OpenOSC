#include "openosc/board/current.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/input.hpp"
namespace openosc {
namespace {
constexpr board::InputPin PowerKey = board::Current::PowerKey;
constexpr uint32_t PowerMask = PowerKey.pin.mask();
constexpr bool PowerOnPortA = PowerKey.pin.port == board::Port::A;
constexpr uint32_t PowerPull = PowerKey.pull == board::Pull::Down ? GPIO_PULLDOWN
    : PowerKey.pull == board::Pull::Up ? GPIO_PULLUP : GPIO_NOPULL;
static_assert(PowerOnPortA || PowerKey.pin.port == board::Port::C);
}
void Keypad::initialize() {
    GPIO_InitTypeDef gpio{};
    gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLUP;
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOB, &gpio);
#ifdef OPENOSC_APPLICATION
    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = PowerMask; gpio.Pull = PowerPull;
    HAL_GPIO_Init(PowerOnPortA ? GPIOA : GPIOC, &gpio);
#endif
}
uint32_t Keypad::read() {
    const uint32_t c = GPIOC->IDR, b = GPIOB->IDR;
    uint32_t value = (!(c & GPIO_PIN_2) ? uint32_t(F1) : 0U) | (!(c & GPIO_PIN_3) ? uint32_t(F2) : 0U)
        | (!(c & GPIO_PIN_5) ? uint32_t(F3) : 0U) | (!(b & GPIO_PIN_4) ? uint32_t(F4) : 0U)
        | (!(b & GPIO_PIN_7) ? uint32_t(Menu) : 0U)
        | (!(b & GPIO_PIN_3) ? uint32_t(Up) : 0U)
        | (!(b & GPIO_PIN_6) ? uint32_t(Down) : 0U)
        | (!(c & GPIO_PIN_1) ? uint32_t(Left) : 0U)
        | (!(b & GPIO_PIN_14) ? uint32_t(Right) : 0U);
#ifdef OPENOSC_APPLICATION
    const uint32_t a = GPIOA->IDR;
    value |= (!(a & GPIO_PIN_0) ? uint32_t(Mode) : 0U)
        | (!(a & GPIO_PIN_2) ? uint32_t(HoldSave) : 0U)
        | (!(b & GPIO_PIN_2) ? uint32_t(AutoRange) : 0U)
        | (((PowerOnPortA ? a : c) & PowerMask) ? uint32_t(Relative) : 0U);
#endif
    boot_diagnostics.keys = value;
    return value;
}
uint32_t Keypad::pressed() {
    static uint32_t candidate = 0xffffffff, stable = 0xffffffff, since = 0;
    uint32_t value = read(), now = Board::milliseconds();
    if (value != candidate) { candidate = value; since = now; }
    if (candidate != stable && now - since >= 25) {
        uint32_t result = candidate & ~stable; stable = candidate; return result;
    }
    return 0;
}
}
