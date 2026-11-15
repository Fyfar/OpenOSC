#include "openosc/platform/watchdog.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/power.hpp"
#include "openosc/platform/control_latch.hpp"
#include "openosc/platform/interrupts.hpp"
#include "openosc/platform/soft_off.hpp"
#include "openosc/platform/system_reset.hpp"
#include "openosc/board/current.hpp"
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
namespace {
// The power key as a plain input without pull, as the factory configures it (PC10 on the ZT703S, PA6 on
// the ZT706S). Recovery has not set it up (Keypad::initialize does only in the application).
constexpr auto PowerKey = board::Current::PowerKey;
static_assert(PowerKey.pull == board::Pull::None, "The power key is read without a pull");
GPIO_TypeDef *powerKeyPort() { return PowerKey.pin.port == board::Port::A ? GPIOA : GPIOC; }
static_assert(PowerKey.pin.port == board::Port::A || PowerKey.pin.port == board::Port::C);
void configurePowerKey() {
    GPIO_InitTypeDef gpio{};
    gpio.Pin = PowerKey.pin.mask(); gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(powerKeyPort(), &gpio);
}
bool powerKeyHeld() { return powerKeyPort()->IDR & PowerKey.pin.mask(); } // Active high.
} // namespace
// The latch is released. On battery that is the end; on external power the MCU may keep running: then
// it stays dark in soft off (SoftOff, docs/POWER.md) with interrupts masked, timed by polling SysTick's
// COUNTFLAG (no interrupt, no WFI), fed only by Watchdog::terminalFeed, until a deliberate power-key
// hold restarts it through a normal reset (the boot sets the latch again).
[[noreturn]] void PowerControl::powerOff() {
    backlight(false);
    ControlLatch::write(0); // Clear both power-enable bits and disable the analog outputs.
    __disable_irq();
    configurePowerKey();
    SysTick->CTRL = 0;
    SysTick->LOAD = SystemCoreClock / 1000 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Counting, no interrupt.
    SoftOff off;
    while (true) {
        Watchdog::terminalFeed();
        if (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) continue; // Reading CTRL clears the flag.
        if (off.tick(powerKeyHeld())) SystemReset::now();
    }
}
}
