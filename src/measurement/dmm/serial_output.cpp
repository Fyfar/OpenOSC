#include "openosc/measurement/dmm/serial_output.hpp"
#include "openosc/measurement/dmm/diagnostic_format.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/dma_stream.hpp"

namespace openosc::dmm {
namespace {
alignas(32) char transmit[64] __attribute__((section(".dma")));
constexpr uint32_t Flags=0x3dU<<6,Complete=0x20U<<6,Errors=0x0dU<<6;
}
bool SerialOutput::start(uint32_t sequence) {
    if (enabled()) return true;
    if (!resources_.acquire(PeripheralResources::DmmSerialDma|PeripheralResources::OutputJack,lease_)) {
        status_=DataStatus::Busy; return false;
    }
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection=RCC_PERIPHCLK_USART6;
    clock.Usart16ClockSelection=RCC_USART16CLKSOURCE_PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&clock)!=HAL_OK) { lease_.release(); status_=DataStatus::HardwareError; return false; }
    __HAL_RCC_USART6_CLK_ENABLE(); __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_USART6_FORCE_RESET(); __HAL_RCC_USART6_RELEASE_RESET();
    USART6->PRESC=0; USART6->BRR=(HAL_RCC_GetPCLK2Freq()+57600)/115200;
    USART6->CR2=0; USART6->CR3=0;
    USART6->CR1=USART_CR1_UE|USART_CR1_TE;
    GPIO_InitTypeDef pin{};
    pin.Pin=GPIO_PIN_6; pin.Mode=GPIO_MODE_AF_PP; pin.Pull=GPIO_NOPULL;
    pin.Speed=GPIO_SPEED_FREQ_MEDIUM; pin.Alternate=GPIO_AF7_USART6;
    HAL_GPIO_Init(GPIOC,&pin);
    DMAMUX1_Channel13->CCR=DMA_REQUEST_USART6_TX;
    sequence_=sequence; busy_=sent_=false; status_=DataStatus::Ok;
    return true;
}
bool SerialOutput::stop() {
    if (!enabled()) return true;
    const uint32_t start=Board::milliseconds();
    while (busy_ && !(USART6->ISR&USART_ISR_TC) && uint32_t(Board::milliseconds()-start)<10) Board::service();
    USART6->CR3&=~USART_CR3_DMAT;
    if (!DmaStream::disable(DMA2_Stream5)) { status_=DataStatus::HardwareError; return false; }
    USART6->CR1=0; DMA2->HIFCR=Flags;
    GPIO_InitTypeDef pin{}; pin.Pin=GPIO_PIN_6; pin.Mode=GPIO_MODE_ANALOG; pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC,&pin);
    lease_.release(); busy_=false; status_=DataStatus::Ok;
    return true;
}
void SerialOutput::service(const Result &result,uint32_t now) {
    if (!enabled()) return;
    if (busy_) {
        if (DMA2->HISR&Errors) { stop(); status_=DataStatus::HardwareError; return; }
        if (!(DMA2->HISR&Complete) || !(USART6->ISR&USART_ISR_TC)) {
            if (uint32_t(now-sentAt_)>=100) { stop(); status_=DataStatus::Timeout; }
            return;
        }
        USART6->CR3&=~USART_CR3_DMAT;
        DMA2->HIFCR=Flags; busy_=false;
    }
    if (result.sequence==sequence_ || (sent_ && uint32_t(now-sentAt_)<333)) return;
    sequence_=result.sequence;
    const size_t bytes=DiagnosticFormat::format(result,transmit,sizeof transmit);
    if (!bytes) return;
    if (!DmaStream::disable(DMA2_Stream5)) { status_=DataStatus::HardwareError; return; }
    DMA2->HIFCR=Flags;
    DMA2_Stream5->PAR=reinterpret_cast<uint32_t>(&USART6->TDR);
    DMA2_Stream5->M0AR=reinterpret_cast<uint32_t>(transmit);
    DMA2_Stream5->NDTR=bytes;
    DMA2_Stream5->FCR=DMA_SxFCR_DMDIS|DMA_SxFCR_FTH;
    DMA2_Stream5->CR=DmaStream::UartProtocol|DMA_SxCR_MINC|DMA_SxCR_DIR_0|(1U<<16);
    USART6->ICR=USART_ICR_TCCF;
    __DMB(); DMA2_Stream5->CR|=DMA_SxCR_EN;
    // TXE is already asserted between packets. Prefill before exposing that
    // request, otherwise this DMA can flag FIFO underrun on its first byte.
    unsigned wait=0;
    while ((DMA2_Stream5->FCR&DMA_SxFCR_FS)==DMA_SxFCR_FS_2 && wait<10000) ++wait;
    if (wait==10000) { stop(); status_=DataStatus::HardwareError; return; }
    __DSB(); USART6->CR3|=USART_CR3_DMAT;
    busy_=sent_=true; sentAt_=now;
}
} // namespace openosc::dmm
