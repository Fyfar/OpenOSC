#pragma once
#include "openosc/acquisition/arena.hpp"
#include <cstdint>

#ifdef OPENOSC_DIAG_MAILBOX
extern "C" struct OpenoscFirstFault;
#endif

namespace openosc::scope {
// Runs the copies of a split capture ring (CaptureBuffer::split, HistorySchedule) without the CPU.
// DMAMUX1 channel 1 (lane 0) raises an event every HistorySchedule::EventRequests requests; request
// generator 0 turns it into one request of the idle stream DMA1 S2, which completes once a block
// (copyTransfers / EventRequests transfers) and then raises its own DMAMUX event; generators 1-2 turn
// that into one request each of DMA2 S6 and DMA1 S3. Every pacing transfer moves one preloaded AHB
// word into PAR of DMA1 S7, an owned stream that is never enabled, so none touches AXI SRAM, an APB
// peripheral or the SRD bridge of the GPIOD reads. Each pacing TC triggers the MDMA channel of its
// lane, which copies one block (TRGM block) and clears the TC with its mask write; lane 1's TC also
// triggers lane 3's channel (the same TSEL, triggered in parallel; the lower physical channel, lane
// 3's, acknowledges: RM0455 14.5.12). So every lane's copy comes at its block instant, with a whole
// block to its deadline, and no DMAMUX synchronization is used. Lane i is copied by physical channel
// {3, 1, 2, 0}[i]. The linked-list nodes are in AXI SRAM: the MDMA cannot fetch them from DTCM (HIL
// 2026-11-10, CESR TELD).
class HistoryCopy {
public:
    // Programs and enables everything before the lanes start: nothing moves until lane 0 does.
    void arm(const CaptureBuffer &buffer);
    // After the lanes stopped (no more pacing events): stops the request generators, lets a copy
    // already requested finish, disables the MDMA channels and only then reads where each one
    // stopped, then stops the pacing streams. A pacing TC that comes late can therefore copy nothing
    // after the reading. False on an MDMA error, a generator overrun (a trigger lost), a channel that
    // would not stop or one stopped inside a block (its CDAR is not on a block); else `served` = the
    // triggers each lane's channel served modulo HistorySchedule::cycle (HistorySchedule::window
    // checks every lane on its own).
    bool finish(uint32_t (&served)[4]);
    // Stops the MDMA first (disabling a pacing stream sets its TC), then the pacing. True once every
    // MDMA channel and pacing stream is proven disabled; a disarmed copy is stopped already.
    bool stop();
    bool armed() const { return armed_; }
    // The ring word lane `lane`'s MDMA channel reads next (its CSAR), modulo the lane ring.
    uint32_t source(unsigned lane) const;
    // How many words lane `lane`'s copy reads ahead of the lane's DMA writes, CSAR read before NDTR
    // so that a lane moving between the two reads makes it smaller, never larger. Normal is within
    // HistorySchedule::Lead blocks to Lead + 1 blocks and 2 words; a copy behind its lane shows as
    // nearly a whole ring. One sample: it proves nothing about the times between samples.
    uint32_t lead(unsigned lane, const volatile uint32_t &ndtr) const;
    // MDMA channels 0-3 that report a transfer error (bit per lane); `error` gets the first's CESR.
    static uint32_t failed(uint32_t &error);
    static void clearInterrupts();
#ifdef OPENOSC_DIAG_MAILBOX
    // Diagnostic builds: the copies' part of the first-fault record (first_fault.hpp): the pacing streams,
    // each lane's MDMA channel (by lane), the lane -> physical channel map and DMAMUX. Reads only.
    static void snapshot(OpenoscFirstFault &record);
#endif

private:
    bool armed_ = false;
    CaptureBuffer buffer_;
};
} // namespace openosc::scope
