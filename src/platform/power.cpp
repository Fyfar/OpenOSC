#include "openosc/platform/watchdog.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/power.hpp"
#include "openosc/platform/control_latch.hpp"
#include "openosc/platform/interrupts.hpp"
namespace openosc {
void PowerControl::brightness(unsigned percent) {
    TIM15->CCR1 = (percent > 100 ? 100 : percent) * 10;
}
namespace {
uint32_t backlightPrescaler() { return TimerClock::apb2() / 1000000 - 1; } // 1 MHz counter, ARR 999: 1 kHz.
}
void PowerControl::retime() {
    TIM15->PSC = backlightPrescaler(); // Preloaded: the duty (CCR1) is kept, the period follows at the update.
    TIM15->EGR = TIM_EGR_UG;
}
void PowerControl::backlight(bool enabled) {
    __HAL_RCC_TIM15_CLK_ENABLE();
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_1; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF4_TIM15; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    TIM15->PSC = backlightPrescaler(); TIM15->ARR = 999;
    TIM15->CCMR1 = (6U << 4) | TIM_CCMR1_OC1PE;
    // PA1 AF4 is CH1N; enabling CH1 does not drive the backlight pin.
    TIM15->CCER = TIM_CCER_CC1NE; TIM15->BDTR = TIM_BDTR_MOE;
    TIM15->CCR1 = enabled ? 650 : 0;
    TIM15->EGR = TIM_EGR_UG; TIM15->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
}
[[noreturn]] void PowerControl::powerOff() {
    backlight(false);
    ControlLatch::write(0); // Clear both power-enable bits and disable the analog outputs.
    __disable_irq();
    Watchdog::park();
}
}
