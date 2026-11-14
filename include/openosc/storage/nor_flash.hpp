#pragma once
#include "openosc/storage/spi_bus.hpp"

namespace openosc {
struct NorGeometry {
    uint32_t jedecId = 0;
    uint32_t capacity = 0;
    uint16_t pageSize = 256;
    uint16_t eraseSize = 4096;
    bool sfdp = false;
    bool fourByte = false;
};

class NorFlash {
public:
    bool initialize();
    bool read(uint32_t address, void* data, size_t size);
    bool writeSector(uint32_t address, const uint8_t* data);
    bool ready(uint32_t timeoutMs = 5000);
    bool restoreLegacyAddressing();
    const NorGeometry& geometry() const { return geometry_; }
    const char* error() const { return error_; }
private:
    bool probeSfdp();
    bool readSfdp(uint32_t address, void* data, size_t size);
    bool writeEnable();
    bool addressed(uint8_t opcode, uint32_t address, const uint8_t* write,
                   uint8_t* read, size_t size);
    bool fail(const char* message);
    SpiBus bus_;
    NorGeometry geometry_;
    const char* error_ = "NOR NOT INITIALIZED";
    bool enteredFourByte_ = false;
    bool sfdpPresent_ = false;
};
}

