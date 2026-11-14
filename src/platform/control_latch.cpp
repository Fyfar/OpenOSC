#include "openosc/platform/control_latch.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/critical_section.hpp"
#include "openosc/platform/usb_trace.hpp"

namespace openosc {
namespace {
// One half-period of the external shift clock. A shift stays short and uninterrupted so
// that the register's outputs, including the power-hold bits, ripple as briefly as possible.
volatile uint32_t settleLoops = 8;
void settle() {
    for (volatile unsigned wait = 0; wait < settleLoops; ++wait)
        __NOP();
}
} // namespace
void ControlLatch::write(uint16_t value) {
    // Every shift disturbs the instrument's supply (see settle() above), so an
    // unchanged word is never resent.
    static uint16_t applied = 0;
    static bool appliedValid = false;
    if (appliedValid && applied == value) return;
    applied = value;
    appliedValid = true;
    usbtrace::mark(usbtrace::Tag::LatchBegin, value);
    __HAL_RCC_GPIOE_CLK_ENABLE();
    constexpr uint32_t pins = GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_7;
    GPIOE->BSRR = pins << 16;
    GPIO_InitTypeDef gpio{};
    gpio.Pin = pins;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
    // An interrupt inside the shift stretches the time the register's outputs ripple.
    CriticalSection shifting;
    for (int bit = 15; bit >= 0; --bit) {
        GPIOE->BSRR = uint32_t(GPIO_PIN_7) << 16;
        GPIOE->BSRR = value & (1U << bit) ? GPIO_PIN_1 : uint32_t(GPIO_PIN_1) << 16;
        settle();
        GPIOE->BSRR = GPIO_PIN_7;
        settle();
    }
    GPIOE->BSRR = uint32_t(GPIO_PIN_7) << 16;
    GPIOE->BSRR = GPIO_PIN_3;
    settle();
    GPIOE->BSRR = uint32_t(GPIO_PIN_3) << 16;
    usbtrace::mark(usbtrace::Tag::LatchEnd);
}
} // namespace openosc
