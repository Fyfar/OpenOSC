#pragma once
#include "stm32h7xx_hal.h"
#include <cstddef>

namespace openosc {
// Fixed board allocation: SPI4 RX=DMA2 stream3, TX=stream4. Buffers live
// in uncached AXI SRAM because FatFs callers may supply DTCM stack data.
class SpiDma {
public:
    void initialize();
    bool transfer(const uint8_t *write,uint8_t *read,size_t size);
};
} // namespace openosc
