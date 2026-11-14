#include "openosc/storage/nor_flash.hpp"
#include "openosc/platform/board.hpp"

namespace openosc {
namespace {
uint32_t word(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8)
        | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}
bool knownManufacturer(uint8_t value) {
    return value == 0xef || value == 0xc8 || value == 0xc2
        || value == 0x20 || value == 0x1c || value == 0x85;
}
}

bool NorFlash::readSfdp(uint32_t address, void* data, size_t size) {
    const uint8_t command[] = {0x5a, uint8_t(address >> 16), uint8_t(address >> 8), uint8_t(address), 0};
    return bus_.transaction(command, sizeof command, nullptr, static_cast<uint8_t*>(data), size);
}

bool NorFlash::probeSfdp() {
    uint8_t header[8];
    if (!readSfdp(0, header, sizeof header)) return false;
    if (word(header) != 0x50444653) return false;
    sfdpPresent_ = true;
    if (header[5] != 1 || header[6] > 15) return false;
    for (unsigned i = 0; i <= header[6]; ++i) {
        uint8_t parameter[8];
        if (!readSfdp(8 + 8 * i, parameter, sizeof parameter)) return false;
        if (parameter[0] != 0 || parameter[7] != 0xff || parameter[3] < 9) continue;
        uint32_t pointer = uint32_t(parameter[4]) | (uint32_t(parameter[5]) << 8) | (uint32_t(parameter[6]) << 16);
        uint8_t table[64]{};
        size_t length = parameter[3] > 16 ? 64 : 4 * parameter[3];
        if (!readSfdp(pointer, table, length)) return false;
        uint32_t density = word(table + 4);
        uint64_t bits = (density & 0x80000000U)
            ? ((density & 0x7fffffffU) < 35 ? (uint64_t(1) << (density & 0x7fffffffU)) : 0)
            : uint64_t(density) + 1;
        if (bits < 4ULL * 1024 * 1024 * 8 || bits > 256ULL * 1024 * 1024 * 8 || (bits & 7)) return false;
        bool erase4k = false;
        for (unsigned entry = 0; entry < 4; ++entry) {
            const auto* erase = table + 28 + entry * 2;
            if (erase[0] == 12 && erase[1] == 0x20) erase4k = true;
        }
        if (!erase4k) return false;
        if (length >= 44) {
            unsigned exponent = (word(table + 40) >> 4) & 15;
            if (exponent && exponent != 8) return false;
        }
        geometry_.capacity = bits / 8;
        geometry_.sfdp = true;
        return true;
    }
    return false;
}

bool NorFlash::initialize() {
    if (!bus_.initialize()) return fail("SPI4 INITIALIZATION FAILED");
    const uint8_t command = 0x9f;
    uint8_t id[3];
    if (!bus_.transaction(&command, 1, nullptr, id, sizeof id)) return fail("NOR ID READ FAILED");
    geometry_.jedecId = (uint32_t(id[0]) << 16) | (uint32_t(id[1]) << 8) | id[2];
    boot_diagnostics.jedecId = geometry_.jedecId;
    if (!probeSfdp()) {
        if (sfdpPresent_) return fail("UNSUPPORTED SFDP GEOMETRY");
        const bool knownType = id[1] == 0x20 || id[1] == 0x40 || id[1] == 0x60;
        if (!knownManufacturer(id[0]) || !knownType || id[2] < 0x16 || id[2] > 0x1c)
            return fail("UNSUPPORTED NOR ID OR GEOMETRY");
        geometry_.capacity = uint32_t(1) << id[2];
    }
    if (geometry_.capacity > 16 * 1024 * 1024) {
        if (!knownManufacturer(id[0])) return fail("UNVERIFIED NOR 4 BYTE MODE");
        if (!writeEnable()) return false;
        const uint8_t enter = 0xb7;
        if (!bus_.transaction(&enter, 1, nullptr, nullptr, 0)) return fail("NOR ADDRESS MODE FAILED");
        geometry_.fourByte = true;
        enteredFourByte_ = true;
    }
    if (!ready()) return false;
    boot_diagnostics.norBytes = geometry_.capacity;
    error_ = nullptr;
    return true;
}
}

