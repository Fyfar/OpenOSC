#pragma once
#include "stm32h7xx.h"

namespace openosc {
class DmaStream {
  public:
    // ES0478 §2.15.3: reserved bit20 selects the alternative request protocol.
    // Apply only to streams servicing USART/UART, never to ADC/GPIO/SPI copies.
    static constexpr uint32_t UartProtocol = 1U << 20;
    static bool disable(DMA_Stream_TypeDef *stream) {
        const bool drain=(stream->FCR&DMA_SxFCR_DMDIS) &&
                         (stream->CR&DMA_SxCR_DIR)!=DMA_SxCR_DIR_0;
        stream->CR &= ~DMA_SxCR_EN;
        for (unsigned wait = 0; wait < 10000; ++wait) {
            // P2M/M2M FIFO writes can finish after EN reads zero. Do not
            // publish their destination or release its owner until empty.
            if (!(stream->CR & DMA_SxCR_EN) && (!drain || (stream->FCR&DMA_SxFCR_FS)==DMA_SxFCR_FS_2)) {
                __DSB();
                return true;
            }
        }
        return false;
    }
};
} // namespace openosc
