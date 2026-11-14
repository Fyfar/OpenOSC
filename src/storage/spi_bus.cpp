#include "openosc/storage/spi_bus.hpp"
#include "openosc/platform/board.hpp"

namespace openosc {
bool SpiBus::initialize() {
    __HAL_RCC_SPI4_CLK_ENABLE();
    __HAL_RCC_SPI4_FORCE_RESET();
    __HAL_RCC_SPI4_RELEASE_RESET();
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
#ifdef OPENOSC_APPLICATION
    clock.Spi45ClockSelection=RCC_SPI45CLKSOURCE_HSI; // Fixed64 MHz across core200/280 profiles.
#else
    clock.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PCLK2;
#endif
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) return false;
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI4;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOE, &gpio);
    handle_.Instance = SPI4;
    handle_.Init.Mode = SPI_MODE_MASTER;
    handle_.Init.Direction = SPI_DIRECTION_2LINES;
    handle_.Init.DataSize = SPI_DATASIZE_8BIT;
    handle_.Init.CLKPolarity = SPI_POLARITY_HIGH;
    handle_.Init.CLKPhase = SPI_PHASE_2EDGE;
    handle_.Init.NSS = SPI_NSS_SOFT;
#ifdef OPENOSC_APPLICATION
    handle_.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_8;
#else
    handle_.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
#endif
    handle_.Init.FirstBit = SPI_FIRSTBIT_MSB;
    handle_.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    handle_.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    handle_.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    handle_.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    if (HAL_SPI_Init(&handle_)!=HAL_OK) return false;
#ifdef OPENOSC_APPLICATION
    dma_.initialize();
#endif
    return true;
}

bool SpiBus::transaction(const uint8_t* command, uint16_t commandSize,
                         const uint8_t* write, uint8_t* read, size_t size) {
    if (!command || !commandSize || (size && !write && !read) || (write && read)) return false;
    GPIOE->BSRR = uint32_t(GPIO_PIN_4) << 16;
    bool success =
#ifdef OPENOSC_APPLICATION
        dma_.transfer(command,nullptr,commandSize);
#else
        HAL_SPI_Transmit(&handle_, const_cast<uint8_t*>(command), commandSize, 100) == HAL_OK;
#endif
    while (success && size) {
        const uint16_t chunk = size > 4096 ? 4096 : size;
        if (write) {
#ifdef OPENOSC_APPLICATION
            success=dma_.transfer(write,nullptr,chunk);
#else
            success = HAL_SPI_Transmit(&handle_, const_cast<uint8_t*>(write), chunk, 100) == HAL_OK;
#endif
            write += chunk;
        } else {
#ifdef OPENOSC_APPLICATION
            success=dma_.transfer(nullptr,read,chunk);
#else
            success = HAL_SPI_Receive(&handle_, read, chunk, 100) == HAL_OK;
#endif
            read += chunk;
        }
        size -= chunk;
        Board::service();
    }
    GPIOE->BSRR = GPIO_PIN_4;
    return success;
}
}

