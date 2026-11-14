#include "openosc/generator/output.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/interrupts.hpp"

namespace openosc::generator {
namespace {
alignas(32) uint16_t tables[2][1024] __attribute__((section(".generator")));
}
bool Output::configure(const Settings &settings) {
    if (configured_ && settings.valid() && settings_.same(settings)) return true;
    OutputPlan plan;
    if (!OutputPlan::calculate(settings, TimerClock::apb1(), plan))
        return false;
    const unsigned next = activeBank_ ^ 1;
    if (!plan.pwm && !WaveTable::build(settings, tables[next], plan.count))
        return false;
    const bool wasEnabled = enabled_;
    if (wasEnabled && !halt())
        return false;
    const auto previousSettings = settings_;
    const auto previousPlan = plan_;
    const auto previousBank = activeBank_;
    const bool wasConfigured = configured_;
    settings_ = settings;
    plan_ = plan;
    activeBank_ = next;
    configured_ = true;
    if (wasEnabled) {
        if (apply()) return true;
        const auto failure = status_;
        if (!halt()) return false;
        settings_ = previousSettings; plan_ = previousPlan; activeBank_ = previousBank; configured_ = wasConfigured;
        if (wasConfigured && apply()) status_ = failure;
        else status_ = DataStatus::HardwareError;
        return false;
    }
    status_ = DataStatus::Ok;
    return true;
}
bool Output::halt() {
    TIM6->CR1 &= ~TIM_CR1_CEN;
    TIM13->CR1 &= ~TIM_CR1_CEN;
    TIM13->CCER = 0;
    DAC2->CR = 0;
    BDMA2_Channel0->CCR &= ~BDMA_CCR_EN;
    for (unsigned wait = 0; (BDMA2_Channel0->CCR & BDMA_CCR_EN) && wait < 10000; ++wait)
        __NOP();
    if (BDMA2_Channel0->CCR & BDMA_CCR_EN) {
        status_ = DataStatus::HardwareError;
        return false;
    }
    BDMA2->IFCR = BDMA_FLAG_GL0;
    __DSB();
    enabled_ = false;
    GPIO_InitTypeDef pin{};
    pin.Pin = GPIO_PIN_6;
    pin.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &pin);
    return true;
}
bool Output::apply() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM6_CLK_ENABLE();
    __HAL_RCC_TIM13_CLK_ENABLE();
    __HAL_RCC_DAC2_CLK_ENABLE();
    __HAL_RCC_BDMA2_CLK_ENABLE();
    if (!halt())
        return false;
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_6;
    gpio.Pull = GPIO_NOPULL;
    if (plan_.pwm) {
        TIM13->CR1 = 0;
        TIM13->PSC = plan_.timer.prescaler;
        TIM13->ARR = plan_.timer.reload;
        TIM13->CCR1 = plan_.compare;
        TIM13->CCMR1 = (6U << 4) | TIM_CCMR1_OC1PE;
        if (settings_.dutyPermille == 0)
            TIM13->CCMR1 = 4U << 4;
        if (settings_.dutyPermille == 1000)
            TIM13->CCMR1 = 5U << 4;
        TIM13->CCER = TIM_CCER_CC1E;
        TIM13->EGR = TIM_EGR_UG;
        TIM13->SR = 0;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF9_TIM13;
        HAL_GPIO_Init(GPIOA, &gpio);
        TIM13->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    } else {
        gpio.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &gpio);
        TIM6->CR1 = 0;
        TIM6->PSC = plan_.timer.prescaler;
        TIM6->ARR = plan_.timer.reload;
        TIM6->CR2 = TIM_CR2_MMS_1;
        TIM6->EGR = TIM_EGR_UG;
        TIM6->SR = 0;
        DAC2->MCR = 0;
        DAC2->DHR12R1 = tables[activeBank_][0];
        DMAMUX2_Channel0->CCR = BDMA_REQUEST_DAC2_CH1;
        BDMA2_Channel0->CPAR = reinterpret_cast<uint32_t>(&DAC2->DHR12R1);
        BDMA2_Channel0->CM0AR = reinterpret_cast<uint32_t>(tables[activeBank_]);
        BDMA2_Channel0->CNDTR = plan_.count;
        BDMA2_Channel0->CCR =
            BDMA_CCR_DIR | BDMA_CCR_CIRC | BDMA_CCR_MINC | BDMA_CCR_PSIZE_0 | BDMA_CCR_MSIZE_0;
        DAC2->SR = DAC_SR_DMAUDR1;
        DAC2->CR = DAC_CR_EN1 | DAC_CR_TEN1 | DAC_CR_TSEL1_0 | DAC_CR_TSEL1_2 | DAC_CR_DMAEN1;
        __DMB();
        BDMA2_Channel0->CCR |= BDMA_CCR_EN;
        Board::delay(1);
        TIM6->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    }
    enabled_ = true;
    status_ = DataStatus::Ok;
    return true;
}
bool Output::enable() {
    if (enabled_)
        return true;
    if (!configured_ && !configure(settings_))
        return false;
    if (!lease_.valid() && !resources_.acquire(PeripheralResources::GeneratorDma|PeripheralResources::OutputJack, lease_)) {
        status_ = DataStatus::Busy;
        return false;
    }
    return apply();
}
void Output::disable() {
    if (lease_.valid()) {
        if (!halt())
            return;
        lease_.release();
    }
    status_ = DataStatus::Ok;
}
void Output::service() {
    if (enabled_ && !plan_.pwm && ((BDMA2->ISR & BDMA_FLAG_TE0) || (DAC2->SR & DAC_SR_DMAUDR1))) {
        if (!halt())
            return;
        lease_.release();
        status_ = DataStatus::HardwareError;
    }
}
} // namespace openosc::generator
