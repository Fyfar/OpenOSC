#include "openosc/instrument/data_access.hpp"

// The capture's share of DMA2 with the NOR SPI streams (StorageAdmission). A fast capture stops
// before a transfer starts, its memory returned; startCapture then waits while the transfer is
// admitted, and the next poll after it starts the capture again at the rate its request plans - no
// half rate, no cooldown, a record is never resampled. Nothing failed: Normal and Single stay armed
// (the new capture is past the arm), a force carries over (startCapture), HOLD's pending wait, the
// records shown and held and their leases (USB, recorder, readout) stay, WAIT is kept.
//
// Which captures are fast: by their DMA transfer rate (points per second / points per transfer
// word; an interleaved CH1 word carries two). Every capture lane's FIFO holds about 4 requests past
// its threshold (AcquisitionArena::fullRate), 16 transfers over the 4 lanes: 16 / rate of headroom
// for a stalled memory write. FE was seen at the full rates (140 MS/s: ~0.11 us of headroom;
// HIL 2026-11-15 also at 100/280 hot switch). SlowTransfers = 10 M/s keeps >= 1.6 us, over 14 times
// that, and lies below every profile's full rate (200/100, 280/140, 360/90, and their halves under
// the DAC stream: 50, 70, 45) - all of those stop. A record at or below it (a 512K two-channel record
// takes >= 26 ms there, the slow ones seconds) keeps capturing through the transfer: restarting it
// would throw away what it has gathered. Roll is far below it.
// False: the capture did not stop, the transfer is refused before it touches its buffers.
namespace openosc {
bool DataAccess::yieldForStorage() {
    if (capture_.state() != scope::FrameMailbox::State::Capturing) return true;
    const auto &m = capture_.metadata();
    if (!fastTransfers(m.samplesPerSecond.value(), scope::pointsPerWord(m.layout))) return true;
    return stopCapture(false);
}
} // namespace openosc
