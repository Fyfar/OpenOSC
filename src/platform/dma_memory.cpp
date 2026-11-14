#include "openosc/platform/dma_memory.hpp"
#include "openosc/platform/board.hpp"

namespace openosc {
void DmaMemory::initialize() {
    // RM0455 §2.2: CD AHB (USB/SPI/snapshot DMA) must not wait behind
    // continuous CPU/DMA2D traffic. LTDC has the tighter scanout deadline.
    GPV->AXI_INI1_READ_QOS=8; GPV->AXI_INI1_WRITE_QOS=8;
    GPV->AXI_INI6_READ_QOS=15;
    __DSB();
    MPU->CTRL = 0;
    for (unsigned region = 0; region < ((MPU->TYPE >> 8) & 255); ++region) {
        MPU->RNR = region;
        MPU->RASR = 0;
    }
    struct Region {
        uint32_t address, size;
    };
    constexpr Region regions[] = {{0x24000000, 19}, {0x30000000, 16}, {0x38000000, 14}};
    for (unsigned i = 0; i < 3; ++i) {
        MPU->RNR = i;
        MPU->RBAR = regions[i].address;
        MPU->RASR = (1U << 28) | (3U << 24) | (1U << 19) | (1U << 18) | (regions[i].size << 1) | 1U;
    }
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    SCB_EnableICache();
    SCB_EnableDCache();
}
} // namespace openosc
