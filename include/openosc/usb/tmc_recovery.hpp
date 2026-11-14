#pragma once
#include <cstdint>

namespace openosc::usb {
// USBTMC 1.0 split transactions (section 4.2.1.1-4.2.1.7): INITIATE_ABORT_BULK_OUT/IN and
// INITIATE_CLEAR with their CHECK_*_STATUS. EP0 requests (the USB interrupt) only answer from
// published state and record the request; the work is done by poll() in the main context:
// - Bulk-OUT (abort OUT, clear): the caller halts the OUT endpoint before the INITIATE response
//   (Tables 20 and 32). poll() then sets the global OUT NAK and waits until it is effective
//   (RM0455 section 62.15.6): every packet received before it is then in memory. The work
//   waits one more poll, with interrupts unmasked in between, and until no OUT completion of
//   the TMC endpoint is pending, so that a DataOut of such a packet has been published before
//   the software buffer is dropped. The shared RxFIFO is never flushed; CDC only sees NAKs.
// - Bulk-IN (abort IN, clear): no new data is generated; packets already queued are sent (USB
//   DMA keeps its buffer), then a short packet ends the transfer if it has none. The endpoint
//   is not disabled and no Tx FIFO is flushed. bmAbortBulkIn/bmClear D0 say what the host can
//   still read and are read live by EP0: an IN transfer the endpoint still holds
//   (Port::bulkInBusy) or a short packet decided but not yet queued (published by the main
//   context with the decision and with the queuing). Not the software buffer ownership: its
//   DataIn may run after the CHECK's SETUP in the same interrupt, and D0 = 1 for a packet the
//   host already read makes it wait for a read that never comes.
// While a split works, the pipe holds the TMC directions it affects (holdsBulkOut/In): no
// input is fed or dispatched, no OUT buffer re-armed, no new IN data generated. epoch()
// changes on every accepted INITIATE.
// A NAK not effective (or a completion still pending) within NakTimeoutMs fails closed: the NAK
// is released, CHECK answers STATUS_FAILED and the TMC pipe stays held (OUT halted, nothing
// re-armed or generated) until a CLEAR succeeds or the bus is reset/reconfigured.
class TmcRecovery {
public:
    enum Status : uint8_t {
        Success = 0x01, Pending = 0x02, Failed = 0x80, TransferNotInProgress = 0x81,
        SplitNotInProgress = 0x82, SplitInProgress = 0x83
    };
    enum class Kind : uint8_t { None, AbortOut, AbortIn, Clear };
    // The USBTMC transfer state EP0 answers from, published by the main context. inActive: a
    // reply open in software; EP0 adds an IN transfer the endpoint still holds (a queued final
    // short packet the host has not read yet).
    struct Snapshot {
        uint8_t outTag = 0, inTag = 0;
        bool outActive = false, inActive = false;
    };
    // The protocol work behind a split transaction (main context).
    class Target {
    public:
        virtual void abortBulkOut() = 0;       // The message being received and its buffer dropped.
        virtual void abortBulkIn() = 0;        // No further data; a short packet if needed.
        virtual void clear() = 0;              // Both directions.
        virtual bool bulkInQueued() const = 0; // The IN buffer not back yet (DataIn) or a short packet to send.
        virtual bool shortPending() const = 0; // A short packet decided but not queued yet.
        virtual uint32_t bulkOutBytes() const = 0;
        virtual uint32_t bulkInBytes() const = 0;
    protected:
        ~Target() = default;
    };
    class Port { // The controller as a split needs it: register reads/writes only, never a wait.
    public:
        virtual void request(bool on) = 0;          // The global OUT NAK.
        virtual bool effective() const = 0;         // It is effective.
        virtual bool completionPending() const = 0; // A TMC OUT transfer-complete not yet handled.
        virtual bool bulkInBusy() const = 0;        // The TMC IN endpoint still holds a transfer (EPENA).
    protected:
        ~Port() = default;
    };
    static constexpr uint32_t NakTimeoutMs = 10;

    // EP0, USB interrupt. INITIATE_*: `tag` is the transfer to abort, `outQueued` that the
    // Bulk-OUT buffer holds unread bytes (Tables 20 and 26); `replyTag` the current or most
    // recent transfer's bTag. After a failure only INITIATE_CLEAR is accepted (ABORT_*: FAILED).
    // On Success for AbortOut/Clear the caller halts Bulk-OUT before queuing the response.
    Status initiate(Kind kind, uint8_t tag, bool outQueued, uint8_t &replyTag);
    // CHECK_*_STATUS of `kind`: `queued` is bmAbortBulkIn/bmClear D0, `count` NBYTES_RXD/TXD.
    Status check(Kind kind, uint8_t &queued, uint32_t &count);
    // Any other USBTMC class request (GET_CAPABILITIES): false while an INITIATE still works
    // (answer it with STATUS_SPLIT_IN_PROGRESS); a finished one's prepared CHECK is discarded.
    bool admit();
    // SET_INTERFACE (Table 17): the INITIATE is aborted; the transfers stay as they are.
    void cancel();
    // Bus reset or (de)configuration (Table 17, rule 7), from the class driver with its new
    // `generation`: the INITIATE is aborted and the cached transfer state is dropped (no
    // transfer, no short packet), as the session is reset for that generation. DMA ownership
    // is the class driver's and not touched here.
    void reset(uint32_t generation);
    // Main context. `generation`: the class generation the caller's Tmc state belongs to
    // (InstrumentUsb::synchronize); state of an older one is never published.
    void publish(const Snapshot &snapshot, uint32_t generation);
    // A short packet is now pending / was queued: published inside the caller's critical
    // section together with the queuing, so that EP0 never sees one without the other.
    void shortPending(bool pending, uint32_t generation) { if (generation == generation_) shortPending_ = pending; }
    void poll(uint32_t now, Port &port, Target &target, uint32_t generation);
    void release(Port &port); // Before the controller is stopped.
    bool idle() const { return kind_ == Kind::None; }
    bool holdsBulkOut() const { return working() || failed_; }
    bool holdsBulkIn() const { return (working() && kind_ != Kind::AbortOut) || failed_; }
    uint32_t epoch() const { return epoch_; }

private:
    enum class Phase : uint8_t { None, OutDrain, Fence, Act, InDrain, Done };
    bool working() const { return kind_ != Kind::None && phase_ != Phase::Done; }
    void finish(Status result, uint32_t count);
    Snapshot snapshot_{};
    volatile Kind kind_ = Kind::None;
    volatile Phase phase_ = Phase::None;
    volatile Status result_ = Success;
    volatile uint32_t count_ = 0, cancels_ = 0, epoch_ = 0, generation_ = 0;
    volatile bool shortPending_ = false, failed_ = false;
    const Port *port_ = nullptr; // Set by poll(), which runs before any split can be accepted.
    bool inBusy() const { return port_ && port_->bulkInBusy(); }
    uint32_t since_ = 0, seenCancels_ = 0;
    bool nakHeld_ = false;
};
extern TmcRecovery tmcRecovery;
} // namespace openosc::usb
