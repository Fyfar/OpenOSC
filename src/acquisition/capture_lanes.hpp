#pragma once
#include "stm32h7xx.h"

namespace openosc::scope::capture_dma {
inline DMA_Stream_TypeDef *const streams[]={DMA1_Stream1,DMA2_Stream1,DMA1_Stream0,DMA2_Stream0};
inline DMAMUX_Channel_TypeDef *const mux[]={DMAMUX1_Channel1,DMAMUX1_Channel9,DMAMUX1_Channel0,DMAMUX1_Channel8};
inline constexpr IRQn_Type irqs[]={DMA1_Stream1_IRQn,DMA2_Stream1_IRQn,DMA1_Stream0_IRQn,DMA2_Stream0_IRQn};
inline uint32_t flags(unsigned bank) { return bank<2 ? 0x3dU<<6 : 0x3dU; }
inline DMA_TypeDef *controller(unsigned bank) { return bank&1 ? DMA2 : DMA1; }
// Lane i's DMA request is TIM3 CH(i+1) (DMAMUX1 request 23 + i, CaptureDma::start): its CCxDE bit.
inline uint32_t request(unsigned lane) { return TIM_DIER_CC1DE << lane; }
// The stop order of RM0455 15.3.19 ("To switch off a peripheral connected to a DMA stream request, it
// is mandatory to, first, switch off the DMA stream to which the peripheral is connected, then to wait
// for EN bit = 0. Only then can the peripheral be safely disabled."), used by the freeze and quiesce:
// 1. stopEvents(): TIM3 makes no new CC event. SMCR 0 first, so that no ITR edge sets CEN again, then
//    CEN 0. Every lane's request enable (DIER CCxDE) stays set: a request already raised stays raised
//    and its stream serves and acknowledges it. All four lanes end on the same counter value.
// 2. Each stream: EN 0, then EN read 0 and its FIFO flushed (DmaStream::disable).
// 3. releaseRequest(lane): only then that lane's CCxDE is cleared. A stream that will not stop keeps
//    its request enable until a retry proves it stopped.
// Clearing DIER first, with every stream enabled and serving requests at the full rate, is what
// preceded the first stream that would not disable in the 512K split HIL of 2026-11-11 (not a proven
// cause; the RM gives the order, not the failure mode).
inline void stopEvents() {
    TIM3->SMCR=0;
    TIM3->CR1&=~TIM_CR1_CEN;
    (void)TIM3->CR1;
    __DSB();
}
inline void releaseRequest(unsigned lane) { TIM3->DIER&=~request(lane); }
} // namespace openosc::scope::capture_dma
