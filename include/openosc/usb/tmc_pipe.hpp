#pragma once
#include "openosc/scpi/tmc.hpp"
#include "openosc/usb/tmc_recovery.hpp"

namespace openosc::usb {
// The USBTMC bulk pipe (OUT 0x01, IN 0x81) between the class driver's buffers and scpi::Tmc,
// and the protocol side of its split transactions (TmcRecovery::Target). Main context only;
// no controller registers (those are behind TmcRecovery::Port).
// While TmcRecovery holds a direction, nothing is fed, dispatched or re-armed (OUT), or
// generated (IN); a short packet and the packet already in the USB DMA still go out. A packet
// generated while a CLEAR or ABORT_IN was accepted is retracted, not queued; one generated
// while an ABORT_OUT was accepted is queued (Bulk-IN is independent of it). A
// malformed USBTMC header halts Bulk-OUT only, so a CLEAR can still drain Bulk-IN.
class TmcPipe final : private TmcRecovery::Target {
public:
    TmcPipe(scpi::Tmc &tmc, scpi::Engine &engine, TmcRecovery &recovery) : tmc_(tmc), engine_(engine), recovery_(recovery) {}
    // One turn: the split's work first, then input, dispatch (`dispatch`: the main poll only,
    // never inside a FAT wait) and output for USB `generation`.
    void service(uint32_t now, uint32_t generation, bool dispatch, TmcRecovery::Port &port);

private:
    bool current(uint32_t generation) const;
    void input(uint32_t now, uint32_t generation);
    void output(uint32_t generation);
    void publish(uint32_t epoch, uint32_t generation);
    void dropBulkOut();
    void abortBulkOut() override;
    void abortBulkIn() override;
    void clear() override;
    bool bulkInQueued() const override;
    bool shortPending() const override { return tmc_.needsZlp(); }
    uint32_t bulkOutBytes() const override { return tmc_.bulkOutBytes(); }
    uint32_t bulkInBytes() const override { return tmc_.bulkInBytes(); }
    scpi::Tmc &tmc_;
    scpi::Engine &engine_;
    TmcRecovery &recovery_;
};
} // namespace openosc::usb
