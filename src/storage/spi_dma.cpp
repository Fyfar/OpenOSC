#ifdef OPENOSC_APPLICATION
#include "openosc/storage/spi_dma.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/dma_stream.hpp"
#include "openosc/platform/cooperative_service.hpp"
#include <cstring>

namespace openosc {
namespace {
alignas(32) uint8_t transmit[4096] __attribute__((section(".dma")));
alignas(32) uint8_t receive[4096] __attribute__((section(".dma")));
constexpr uint32_t RxFlags=0x3dU<<22,TxFlags=0x3dU;
constexpr uint32_t RxComplete=0x20U<<22,TxComplete=0x20U;
constexpr uint32_t RxErrors=0x0dU<<22,TxErrors=0x0dU;
constexpr uint32_t SpiFlags=SPI_IFCR_EOTC|SPI_IFCR_TXTFC|SPI_IFCR_UDRC|SPI_IFCR_OVRC|
    SPI_IFCR_CRCEC|SPI_IFCR_TIFREC|SPI_IFCR_MODFC|SPI_IFCR_TSERFC|SPI_IFCR_SUSPC;
}
void SpiDma::initialize() {
    __HAL_RCC_DMA2_CLK_ENABLE();
    DMAMUX1_Channel11->CCR=DMA_REQUEST_SPI4_RX;
    DMAMUX1_Channel12->CCR=DMA_REQUEST_SPI4_TX;
}
bool SpiDma::transfer(const uint8_t *write,uint8_t *read,size_t size) {
    if (!size || size>sizeof transmit) return false;
    if (write) std::memcpy(transmit,write,size);
    else std::memset(transmit,0xff,size);
    if (!DmaStream::disable(DMA2_Stream3) || !DmaStream::disable(DMA2_Stream4)) return false;
    SPI4->CR1 &= ~SPI_CR1_SPE;
    SPI4->IER=0;
    for (unsigned drained=0; (SPI4->SR & SPI_SR_RXP) && drained<32; ++drained)
        (void)*reinterpret_cast<volatile uint8_t*>(&SPI4->RXDR);
    if (SPI4->SR & SPI_SR_RXP) return false;
    SPI4->IFCR=SpiFlags;
    SPI4->CFG2 &= ~SPI_CFG2_COMM; // Full duplex, including command bytes.
    SPI4->CFG1 &= ~(SPI_CFG1_RXDMAEN|SPI_CFG1_TXDMAEN);
    SPI4->CR1=SPI_CR1_SSI;
    SPI4->CR2=uint32_t(size);
    DMA2->LIFCR=RxFlags; DMA2->HIFCR=TxFlags;
    DMA2_Stream3->PAR=reinterpret_cast<uint32_t>(&SPI4->RXDR);
    DMA2_Stream3->M0AR=reinterpret_cast<uint32_t>(receive);
    DMA2_Stream3->NDTR=size;
    // The FIFO absorbs SPI requests while AXI is arbitrated with acquisition
    // and display DMA. Direct TX mode can raise FE even on a short transfer.
    DMA2_Stream3->FCR=DMA_SxFCR_DMDIS|DMA_SxFCR_FTH;
    DMA2_Stream3->CR=DMA_SxCR_MINC|(3U<<16);
    DMA2_Stream4->PAR=reinterpret_cast<uint32_t>(&SPI4->TXDR);
    DMA2_Stream4->M0AR=reinterpret_cast<uint32_t>(transmit);
    DMA2_Stream4->NDTR=size;
    DMA2_Stream4->FCR=DMA_SxFCR_DMDIS|DMA_SxFCR_FTH;
    DMA2_Stream4->CR=DMA_SxCR_MINC|DMA_SxCR_DIR_0|(3U<<16);
    __DMB();
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    SPI4->CFG1 |= SPI_CFG1_RXDMAEN;
    DMA2_Stream4->CR |= DMA_SxCR_EN;
    SPI4->CFG1 |= SPI_CFG1_TXDMAEN;
    SPI4->CR1 |= SPI_CR1_SPE;
    SPI4->CR1 |= SPI_CR1_CSTART;
    const uint32_t start=Board::milliseconds();
    bool complete=false;
    while (uint32_t(Board::milliseconds()-start)<100) {
        if ((DMA2->LISR & RxErrors) || (DMA2->HISR & TxErrors) ||
            (SPI4->SR & (SPI_SR_OVR|SPI_SR_MODF|SPI_SR_TIFRE))) break;
        if ((DMA2->LISR & RxComplete) && (DMA2->HISR & TxComplete) &&
            (SPI4->SR & (SPI_SR_EOT|SPI_SR_TXC))==(SPI_SR_EOT|SPI_SR_TXC)) {
            complete=true; break;
        }
        Board::service();
        CooperativeService::poll(Board::milliseconds());
    }
    SPI4->CR1 &= ~SPI_CR1_SPE;
    SPI4->CFG1 &= ~(SPI_CFG1_RXDMAEN|SPI_CFG1_TXDMAEN);
    const bool rxStopped=DmaStream::disable(DMA2_Stream3),txStopped=DmaStream::disable(DMA2_Stream4);
    SPI4->IFCR=SpiFlags;
    DMA2->LIFCR=RxFlags; DMA2->HIFCR=TxFlags;
    __DMB();
    if (!complete || !rxStopped || !txStopped) return false;
    if (read) std::memcpy(read,receive,size);
    return true;
}
} // namespace openosc
#endif
