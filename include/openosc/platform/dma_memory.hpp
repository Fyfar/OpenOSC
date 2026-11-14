#pragma once

// Initialized buffers copied from Flash to uncached AXI SRAM by startup.
#define OPENOSC_DMA_DATA __attribute__((section(".dma_data"), aligned(32)))

namespace openosc {
class DmaMemory {
  public:
    static void initialize();
};
} // namespace openosc
