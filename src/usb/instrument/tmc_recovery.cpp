#include "openosc/usb/tmc_recovery.hpp"
#include "openosc/platform/critical_section.hpp"

namespace openosc::usb {
TmcRecovery tmcRecovery;

TmcRecovery::Status TmcRecovery::initiate(Kind kind, uint8_t tag, bool outQueued, uint8_t &replyTag) {
    replyTag = kind == Kind::AbortIn ? snapshot_.inTag : snapshot_.outTag;
    if (!admit()) return SplitInProgress; // Rule 3a: a no-operation.
    if (failed_ && kind != Kind::Clear) return Failed; // Held after a failure: only CLEAR (or a reset).
    if (kind == Kind::AbortOut) {
        if (!snapshot_.outActive) return outQueued ? TransferNotInProgress : Failed; // Table 20.
        if (tag != snapshot_.outTag) return TransferNotInProgress;
    } else if (kind == Kind::AbortIn) {
        if (!snapshot_.inActive && !inBusy()) return outQueued ? TransferNotInProgress : Failed; // Table 26.
        if (tag != snapshot_.inTag) return TransferNotInProgress;
    } else if (kind != Kind::Clear) return Failed;
    count_ = 0;
    phase_ = kind == Kind::AbortIn ? Phase::Act : Phase::OutDrain;
    kind_ = kind;
    epoch_ = epoch_ + 1;
    return Success;
}
bool TmcRecovery::admit() {
    if (kind_ == Kind::None) return true;
    if (phase_ != Phase::Done) return false;
    kind_ = Kind::None; // Rule 3a: the prepared CHECK response is discarded.
    return true;
}
TmcRecovery::Status TmcRecovery::check(Kind kind, uint8_t &queued, uint32_t &count) {
    queued = 0; count = 0;
    if (kind_ == Kind::None) return SplitNotInProgress; // Rule 6.
    if (kind_ != kind) {
        if (phase_ != Phase::Done) return SplitInProgress; // Another class request mid-split.
        kind_ = Kind::None;
        return SplitNotInProgress;
    }
    if (phase_ != Phase::Done) {
        // What the host can still read, never a packet it already has (Tables 29, 35).
        if (kind != Kind::AbortOut) queued = inBusy() || shortPending_ ? 1 : 0;
        return Pending; // NBYTES are 0 while pending (Table 29).
    }
    count = count_;
    kind_ = Kind::None; // The CHECK response ends the split transaction.
    return result_;
}
void TmcRecovery::cancel() {
    kind_ = Kind::None;
    phase_ = Phase::None;
    failed_ = false; // The endpoints are reset with the configuration.
    cancels_ = cancels_ + 1; // Not the epoch: work of an ordinary transfer stays valid.
}
void TmcRecovery::reset(uint32_t generation) {
    cancel();
    snapshot_ = {};
    shortPending_ = false;
    generation_ = generation;
}
void TmcRecovery::publish(const Snapshot &snapshot, uint32_t generation) {
    CriticalSection lock; // Atomic with a reset: an older generation's state is not written back.
    if (generation == generation_) snapshot_ = snapshot;
}
void TmcRecovery::finish(Status result, uint32_t count) {
    count_ = count;
    result_ = result;
    if (result != Success) failed_ = true;          // Held until a CLEAR succeeds or a reset.
    else if (kind_ == Kind::Clear) failed_ = false;
    phase_ = Phase::Done;
}
void TmcRecovery::release(Port &port) {
    if (nakHeld_) { port.request(false); nakHeld_ = false; }
}
void TmcRecovery::poll(uint32_t now, Port &port, Target &target, uint32_t generation) {
    CriticalSection lock; // Each step is a few register accesses; EP0 cannot interleave.
    port_ = &port;
    if (generation != generation_) return; // A pass for an older session: no work, no publication.
    shortPending_ = target.shortPending();
    if (cancels_ != seenCancels_) { // Aborted by EP0: no further work, only the NAK back.
        seenCancels_ = cancels_;
        release(port);
    }
    const Kind kind = kind_;
    switch (phase_) {
    case Phase::OutDrain:
        if (!nakHeld_) { port.request(true); nakHeld_ = true; since_ = now; return; }
        // Effective: packets received before it are in memory. Their DataOut may still be
        // pending behind this critical section: act only on a later poll.
        if (port.effective()) { phase_ = Phase::Fence; return; }
        break;
    case Phase::Fence:
        if (!port.completionPending()) { phase_ = Phase::Act; break; }
        break;
    case Phase::Act: break;
    case Phase::InDrain: // Done once the IN buffer is back (DataIn) and no short packet is left.
        if (!target.bulkInQueued()) finish(Success, kind == Kind::AbortIn ? target.bulkInBytes() : 0);
        return;
    default: return;
    }
    if (phase_ != Phase::Act) {
        if (uint32_t(now - since_) >= NakTimeoutMs) { // Fail closed: OUT stays halted, nothing re-armed.
            release(port);
            finish(Failed, 0);
        }
        return;
    }
    if (kind == Kind::AbortOut) {
        target.abortBulkOut();
        release(port);
        finish(Success, target.bulkOutBytes());
        return;
    }
    if (kind == Kind::Clear) { target.clear(); release(port); }
    else target.abortBulkIn();
    shortPending(target.shortPending(), generation); // Published with the decision.
    phase_ = Phase::InDrain;
    if (!target.bulkInQueued()) finish(Success, kind == Kind::AbortIn ? target.bulkInBytes() : 0);
}
} // namespace openosc::usb
