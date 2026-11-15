#ifdef OPENOSC_APPLICATION
#include "openosc/storage/spi_dma.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/dma_stream.hpp"
#include "openosc/platform/cooperative_service.hpp"
#include "openosc/storage/storage_admission.hpp"
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
// Admitted first (StorageAdmission): the capture yields before the buffers are touched or a stream
// is enabled, and may start again only once both streams are proven stopped and `read` holds the data.
bool SpiDma::transfer(const uint8_t *write,uint8_t *read,size_t size) {
    if (!size || size>sizeof transmit || !StorageAdmission::begin()) return false;
    bool stopped=false;
    const bool done=exchange(write,read,size,stopped);
    StorageAdmission::end(stopped,&SpiDma::stopStreams);
    return done;
}
// Both streams disabled (a P2M FIFO drained) and their flags cleared: the retry of a stop that failed.
bool SpiDma::stopStreams() {
    const bool rxStopped=DmaStream::disable(DMA2_Stream3),txStopped=DmaStream::disable(DMA2_Stream4);
    DMA2->LIFCR=RxFlags; DMA2->HIFCR=TxFlags;
    __DMB();
    return rxStopped && txStopped;
}
bool SpiDma::exchange(const uint8_t *write,uint8_t *read,size_t size,bool &stopped) {
    if (!DmaStream::disable(DMA2_Stream3) || !DmaStream::disable(DMA2_Stream4)) return false;
    stopped=true; // Both idle from here until they are enabled below.
    if (write) std::memcpy(transmit,write,size);
    else std::memset(transmit,0xff,size);
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
    // Memory side in words when the byte count allows it (RM0455 15.3.12, Table 90: PSIZE byte with
    // MSIZE word needs NDT a multiple of 4; packing needs the FIFO, DMDIS): a quarter of the memory-port
    // accesses for the data blocks (4096, 512, 256 bytes), which share DMA2's one memory port with capture
    // lanes 1 and 3. Transfers whose length is not a multiple of four remain byte-wide. The buffers are 32-byte aligned
    // (word M0AR); peripheral-to-memory TCIF follows the FIFO flush (15.3.15), so `receive` holds every
    // word when it is read.
    const uint32_t memoryWidth=size%4==0 ? DMA_SxCR_MSIZE_1 : 0U;
    DMA2_Stream3->PAR=uint32_t(reinterpret_cast<uintptr_t>(&SPI4->RXDR));
    DMA2_Stream3->M0AR=uint32_t(reinterpret_cast<uintptr_t>(receive));
    DMA2_Stream3->NDTR=size;
    // The FIFO absorbs SPI requests while AXI is arbitrated with acquisition
    // and display DMA. Direct TX mode can raise FE even on a short transfer.
    DMA2_Stream3->FCR=DMA_SxFCR_DMDIS|DMA_SxFCR_FTH;
    DMA2_Stream3->CR=DMA_SxCR_MINC|memoryWidth|(3U<<16);
    DMA2_Stream4->PAR=uint32_t(reinterpret_cast<uintptr_t>(&SPI4->TXDR));
    DMA2_Stream4->M0AR=uint32_t(reinterpret_cast<uintptr_t>(transmit));
    DMA2_Stream4->NDTR=size;
    DMA2_Stream4->FCR=DMA_SxFCR_DMDIS|DMA_SxFCR_FTH;
    DMA2_Stream4->CR=DMA_SxCR_MINC|memoryWidth|DMA_SxCR_DIR_0|(3U<<16);
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
    stopped=stopStreams();
    SPI4->IFCR=SpiFlags;
    if (!complete || !stopped) return false;
    if (read) std::memcpy(read,receive,size);
    return true;
}
} // namespace openosc
#endif
