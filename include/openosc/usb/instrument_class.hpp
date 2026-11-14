#pragma once
#include "openosc/platform/critical_section.hpp"
#include "usbd_core.h"
#include <cstddef>

namespace openosc::usb {
inline constexpr size_t TransferBytes = 512;
struct Pipe {
    alignas(4) uint8_t receive[64]{}, transmit[TransferBytes]{};
    volatile uint16_t received = 0;
    uint16_t consumed = 0;
    volatile bool transmitBusy = false, receiveReady = false;
};
struct InstrumentClass {
    Pipe pipes[2];
    volatile bool configured = false;
    volatile uint32_t generation = 0, clearMask = 0;
    alignas(4) uint8_t control[24]{};
    // A host can send a full EP0 packet even for a seven-byte CDC request.
    alignas(4) uint8_t lineCoding[64] = {0x00,0xc2,0x01,0,0,0,8,0};
    bool dtr = false;
};
extern InstrumentClass instrument;
extern USBD_ClassTypeDef instrumentClass;
extern USBD_DescriptorsTypeDef instrumentDescriptors;
void receive(unsigned pipe);
bool transmit(unsigned pipe, size_t size, uint32_t generation);
// The pipe still belongs to the session of `generation`: configured, the same generation, and
// not cleared since (clearMask: a new configuration; DTR for CDC) until the main context's
// synchronize() has reset that session. Call inside a CriticalSection.
bool current(unsigned pipe, uint32_t generation);
// Bulk-OUT 0x01 halted (a malformed USBTMC header) for the session of `generation`.
void haltTmcOut(uint32_t generation);
// The main context's input of `pipe` for the session of `generation`: each received byte is
// checked (current, and `allowed()`), handed to `feed(byte)` and counted in one bounded
// critical section (a parser byte, never a dispatch), so that a reset or a new session's
// packet arriving meanwhile is never fed to the old session or counted in its stead. The
// buffer is re-armed only when wholly consumed in that same session.
template <class Allowed, class Feed> void consume(unsigned index, uint32_t generation, Allowed allowed, Feed feed) {
    auto &p = instrument.pipes[index];
    for (;;) {
        CriticalSection lock;
        if (!current(index, generation) || !allowed() || !p.receiveReady) return;
        if (p.consumed == p.received) { receive(index); return; }
        if (!feed(p.receive[p.consumed])) return; // The parser is full: later.
        ++p.consumed;
    }
}
} // namespace openosc::usb
