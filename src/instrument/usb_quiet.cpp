#include "openosc/instrument/data_access.hpp"

// The capture while a host enumerates (usb::QuietAttach, usb::EnumerationQuiet). quiet: a running
// capture above SlowTransfers stops (stopCapture(false): its memory returned, Normal/Single armed, a
// force carried over, records shown and held and their leases kept, WAIT kept) and startCapture
// holds fast plans back; slow records and Roll go on. The answer is true once no fast capture runs:
// a capture that did not stop (CaptureDma retries its stop; stopCapture is asked again each poll)
// or a pending HOLD (its capture or its retry, at most HoldWaitMs) answers false, and the attach waits.
// quiet false: fast plans start again from the next poll, at the rate their request plans.
// It acts on the capture that runs, whatever the screen shows (GEN over a running Scope included).
namespace openosc {
bool DataAccess::quietCapture(bool quiet) {
    usbQuiet_ = quiet;
    if (!quiet) return true;
    // A HOLD waiting for its record (running, or between a fault and its retry) is never quiet: its
    // capture, and its fast retry, go on until it holds a record or its deadline passes.
    if (holdPending_) return false;
    if (capture_.state() != scope::FrameMailbox::State::Capturing) return true;
    const auto &m = capture_.metadata();
    if (!fastTransfers(m.samplesPerSecond.value(), scope::pointsPerWord(m.layout))) return true;
    return stopCapture(false);
}
} // namespace openosc
