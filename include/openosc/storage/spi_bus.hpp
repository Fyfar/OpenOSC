#pragma once
#include <cstddef>
#include <cstdint>
#include "stm32h7xx_hal.h"
#ifdef OPENOSC_APPLICATION
#include "openosc/storage/spi_dma.hpp"
#endif

namespace openosc {
class SpiBus {
public:
    bool initialize();
    bool transaction(const uint8_t* command, uint16_t commandSize,
                     const uint8_t* write, uint8_t* read, size_t size);
private:
    SPI_HandleTypeDef handle_{};
#ifdef OPENOSC_APPLICATION
    SpiDma dma_;
#endif
};
}

