#pragma once
// Diagnostic builds only (OPENOSC_DIAG_MAILBOX, env firmware_diag): the normal firmware has no record.
#ifdef OPENOSC_DIAG_MAILBOX
#include <cstdint>

// The FIRST capture fault of any kind since boot, kept in DTCM for an SWD read after STOP (candidate12's
// record, as the 2026-11-11 HIL runs read it). Any kind means every captureFault() value: DMA stream flags, stream disable,
// freeze, split window, MDMA error, and so on.
// - The cumulative counter (openosc_capture_faults) keeps only the last value. In candidate11 remap1 it
//   showed 108 failed stream disables, and what started them was lost.
// - Written by captureFault() at the place the fault is detected, before that caller's cleanup
//   (abandon, retry), with interrupts masked. `valid` is written last, after a DMB; the first fault's
//   fields are never written again until the next boot.
// - It snapshots the registers that state the capture's condition at that moment: the four lane
//   streams, the DMA interrupt flags, the pacing streams, every lane's MDMA channel (by lane, its
//   physical channel in laneChannels), DMAMUX, TIM3.
// - The next LaterFaults faults keep their value and time only (`later` counts them, each entry
//   written before the count), so what followed the first one shows too.
// - Recording changes no capture behaviour: the capture retries after a transient fault as the normal
//   firmware does (the earlier diagnostic latch that stopped every later capture is not part of it).
extern "C" struct OpenoscFirstFault {
    volatile uint32_t valid, version, fault, count, millis, cycles, laneChannels, later;
    volatile uint32_t laneCR[4], laneNDTR[4], laneFCR[4], laneM0AR[4]; // DMA1 S1, DMA2 S1, DMA1 S0, DMA2 S0.
    volatile uint32_t dmaISR[4];                                      // DMA1 LISR, DMA1 HISR, DMA2 LISR, DMA2 HISR.
    volatile uint32_t pacerCR[3], pacerNDTR[3];                       // DMA1 S2, DMA2 S6, DMA1 S3.
    volatile uint32_t mdmaCCR[4], mdmaCISR[4], mdmaCESR[4], mdmaCBNDTR[4], mdmaCSAR[4], mdmaCDAR[4], mdmaCLAR[4]; // By lane.
    volatile uint32_t muxChannel1, muxChannel2, muxRGSR, muxCSR;      // Lane 0's event channel, lane 0's pacer's, the flags.
    volatile uint32_t tim3CR1, tim3DIER, tim3SR, tim3SMCR;
    volatile uint32_t laterFault[4], laterMillis[4];
} openosc_first_fault;

namespace openosc::scope {
constexpr uint32_t FirstFaultValid = 0x31464346; // "FCF1"
constexpr uint32_t FirstFaultVersion = 1;
constexpr unsigned LaterFaults = 4;
} // namespace openosc::scope
#endif
