#include "openosc/storage/nor_flash.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/platform/cooperative_service.hpp"
#include <cstring>

namespace openosc {
bool NorFlash::fail(const char* message) { error_ = message; return false; }

bool NorFlash::ready(uint32_t timeoutMs) {
    const uint8_t command = 0x05;
    const uint32_t start = Board::milliseconds();
    uint8_t status = 0;
    do {
        if (!bus_.transaction(&command, 1, nullptr, &status, 1)) return fail("NOR STATUS READ FAILED");
        if (!(status & 1)) return true;
        Board::service();
        CooperativeService::poll(Board::milliseconds());
    } while (Board::milliseconds() - start < timeoutMs);
    return fail("NOR BUSY TIMEOUT");
}

bool NorFlash::writeEnable() {
    if (!ready()) return false;
    const uint8_t command = 0x06, read = 0x05;
    uint8_t status = 0;
    if (!bus_.transaction(&command, 1, nullptr, nullptr, 0)
        || !bus_.transaction(&read, 1, nullptr, &status, 1) || !(status & 2))
        return fail("NOR WRITE ENABLE FAILED");
    return true;
}

bool NorFlash::addressed(uint8_t opcode, uint32_t address, const uint8_t* write,
                         uint8_t* read, size_t size) {
    uint8_t command[5] = {opcode};
    if (opcode != 0x03) usbtrace::mark(usbtrace::Tag::Nor, opcode | (address << 8));
    const unsigned count = geometry_.fourByte ? 4 : 3;
    for (unsigned i = 0; i < count; ++i) command[1 + i] = address >> (8 * (count - i - 1));
    if (!bus_.transaction(command, count + 1, write, read, size)) return fail("NOR SPI TRANSFER FAILED");
    return true;
}

bool NorFlash::read(uint32_t address, void* data, size_t size) {
    if (address > geometry_.capacity || size > geometry_.capacity - address || (!data && size))
        return fail("NOR READ OUT OF RANGE");
    if (!size) return true;
    return ready() && addressed(0x03, address, nullptr, static_cast<uint8_t*>(data), size);
}

bool NorFlash::writeSector(uint32_t address, const uint8_t* data) {
    if ((address & 4095) || address >= geometry_.capacity
        || geometry_.capacity - address < 4096 || !data) return fail("NOR WRITE OUT OF RANGE");
    alignas(4) uint8_t verify[256];
    bool identical = true;
    for (unsigned offset = 0; offset < 4096; offset += 256) {
        if (!read(address + offset, verify, sizeof verify)) return false;
        if (std::memcmp(verify, data + offset, sizeof verify)) identical = false;
    }
    if (identical) return true;
    if (!writeEnable() || !addressed(0x20, address, nullptr, nullptr, 0) || !ready()) return false;
    for (unsigned offset = 0; offset < 4096; offset += 256) {
        bool erased = true;
        for (unsigned i = 0; i < 256; ++i) if (data[offset + i] != 0xff) { erased = false; break; }
        if (!erased && (!writeEnable()
            || !addressed(0x02, address + offset, data + offset, nullptr, 256) || !ready())) return false;
        if (!read(address + offset, verify, sizeof verify)) return false;
        if (std::memcmp(verify, data + offset, sizeof verify)) return fail("NOR WRITE VERIFY FAILED");
        Board::service();
    }
    return true;
}

bool NorFlash::restoreLegacyAddressing() {
    if (!enteredFourByte_) return true;
    const uint8_t exit = 0xe9;
    if (!ready() || !bus_.transaction(&exit, 1, nullptr, nullptr, 0)) return fail("NOR ADDRESS RESTORE FAILED");
    enteredFourByte_ = false;
    geometry_.fourByte = false;
    return true;
}
}
