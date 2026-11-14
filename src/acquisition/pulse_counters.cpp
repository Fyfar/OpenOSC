#include "openosc/acquisition/pulse_counters.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/critical_section.hpp"
#include "openosc/platform/interrupts.hpp"

namespace openosc::scope {
PulseCounters *PulseCounters::active_=nullptr;
namespace {
// External clock mode 2 on ETR (rising edges), no prescaler; the digital filter ETF is CounterWindow's
// level of the window (CounterLadder: 0, 1, 2, 3, 0 per cycle), written only between windows.
uint32_t smcr(uint8_t filter) { return TIM_SMCR_ECE|(uint32_t(filter&15)<<TIM_SMCR_ETF_Pos); }
void pin(GPIO_TypeDef *port,uint32_t pins,uint32_t alternate) {
    GPIO_InitTypeDef gpio{};
    gpio.Pin=pins; gpio.Mode=GPIO_MODE_AF_PP; gpio.Pull=GPIO_NOPULL;
    gpio.Speed=GPIO_SPEED_FREQ_LOW; gpio.Alternate=alternate;
    HAL_GPIO_Init(port,&gpio);
}
void timer(TIM_TypeDef *target,uint32_t maximum) {
    target->CR1=0; target->CR2=0; target->DIER=0; target->SMCR=0; target->CCER=0;
    target->CCMR1=target->CCMR2=0;
    target->PSC=0; target->ARR=maximum; target->EGR=TIM_EGR_UG; target->SR=0;
    target->SMCR=smcr(0);
}
}
bool PulseCounters::start() {
    if (lease_.valid()) return true;
    const uint32_t clock=TimerClock::apb1();
    if (clock<1000000 || clock%1000000) return false;
    kernelHz_=clock;
    if (!resources_.acquire(PeripheralResources::ScopeCounters,lease_)) return false;
    __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
    pin(GPIOA,GPIO_PIN_15,GPIO_AF1_TIM2); pin(GPIOE,GPIO_PIN_0,GPIO_AF2_TIM4);
    timer(TIM2,UINT32_MAX); timer(TIM4,65535);
    TIM5->CR1=0; TIM5->CR2=0; TIM5->DIER=0; TIM5->SMCR=0; TIM5->CCER=0;
    TIM5->CCMR1=TIM5->CCMR2=0;
    TIM5->PSC=clock/1000000-1; TIM5->ARR=UINT32_MAX;
    TIM5->EGR=TIM_EGR_UG; TIM5->SR=0;
    TIM5->CR1=TIM_CR1_ARPE|TIM_CR1_CEN;
    high_=0; active_=this;
    Interrupts::attach(TIM4_IRQn,interrupt,5);
    TIM4->DIER=TIM_DIER_UIE;
    TIM2->CR1=TIM4->CR1=TIM_CR1_ARPE|TIM_CR1_CEN;
    invalidate(Board::milliseconds());
    return true;
}
void PulseCounters::interrupt() {
    if (active_ && (TIM4->SR&TIM_SR_UIF)) {
        ++active_->high_;
        TIM4->SR=~TIM_SR_UIF; // W0C: preserve unrelated pending flags.
    }
}
uint32_t PulseCounters::read(uint32_t (&counts)[2]) const {
    CriticalSection guard;
    const uint32_t began=TIM5->CNT;
    counts[0]=TIM2->CNT;
    const uint32_t high=high_; const uint16_t low=TIM4->CNT;
    const bool pending=TIM4->SR&TIM_SR_UIF;
    const uint16_t after=pending ? TIM4->CNT : low;
    counts[1]=CounterWindow::extend16(high,low,pending,after);
    // TIM5 provides an independent free-running microsecond reference.
    // Take it with the counts, before an acquisition IRQ can delay the caller.
    return began+uint32_t(TIM5->CNT-began)/2;
}
void PulseCounters::filter(uint8_t level) {
    TIM2->SMCR=TIM4->SMCR=smcr(level);
}
void PulseCounters::invalidate(uint32_t now) {
    if (!lease_.valid()) return;
    filter(0); // A cycle starts unfiltered.
    uint32_t counts[2]; const uint32_t cycles=read(counts);
    window_.begin(counts,cycles,now,1000000,kernelHz_);
    polledAt_=now;
}
void PulseCounters::service(uint32_t now) {
    if (!lease_.valid() || uint32_t(now-polledAt_)<10) return;
    polledAt_=now;
    uint32_t counts[2]; uint32_t cycles=read(counts);
    if (!window_.observe(counts,cycles,now)) return;
    // The next window, at its filter level, from counts taken after the change.
    filter(window_.filter());
    cycles=read(counts);
    window_.restart(counts,cycles,now);
}
void PulseCounters::stop() {
    if (!lease_.valid()) return;
    TIM2->CR1=TIM4->CR1=TIM5->CR1=0; TIM4->DIER=0;
    Interrupts::detach(TIM4_IRQn); TIM4->SR=0; active_=nullptr;
    GPIO_InitTypeDef gpio{}; gpio.Mode=GPIO_MODE_INPUT; gpio.Pull=GPIO_NOPULL;
    gpio.Pin=GPIO_PIN_15; HAL_GPIO_Init(GPIOA,&gpio);
    gpio.Pin=GPIO_PIN_0; HAL_GPIO_Init(GPIOE,&gpio);
    lease_.release();
    const uint32_t counts[2]={}; window_.begin(counts,0,0,0);
}
} // namespace openosc::scope
