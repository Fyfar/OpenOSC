#include "openosc/acquisition/history_copy.hpp"
#include "openosc/acquisition/history_schedule.hpp"
#include "openosc/platform/dma_stream.hpp"
#ifdef OPENOSC_DIAG_MAILBOX
#include "openosc/acquisition/first_fault.hpp"
#endif
#include "stm32h7xx.h"

namespace openosc::scope {
namespace {
// RM0455 MDMA linked-list node: CTCR, CBNDTR, CSAR, CDAR, CBRUR, CLAR, CTBR, reserved, CMAR, CMDR.
struct alignas(8) Node { uint32_t tcr, bndtr, sar, dar, brur, lar, tbr, reserved, mar, mdr; };
constexpr unsigned MaximumNodes = 6; // HistorySchedule::nodes: three ramp nodes and three passes.
Node nodes[4][MaximumNodes] __attribute__((section(".capture_nodes")));
// Lane i is copied by physical MDMA channel ChannelIndex[i]. Among equal priorities the MDMA serves the
// lower channel number first (RM0455 14.3.8). With the identity map (candidate9/10, 2026-11-11) both 512K
// split runs were refused, lane 3 two copies short of the schedule; with lanes 0 and 3 swapped the
// published 512K frames (candidate12 first1, candidate13 pack1, candidate17 local1) passed the window
// check. An observation on the ZT703S, not a derived cause: the RM does not describe that loss.
constexpr unsigned ChannelIndex[] = {3, 1, 2, 0};
MDMA_Channel_TypeDef *const channels[] = {MDMA_Channel3, MDMA_Channel1, MDMA_Channel2, MDMA_Channel0};
static_assert(sizeof channels / sizeof *channels == 4, "one channel per lane");
// Three pacing streams (lanes 0-2), each with its DMAMUX1 channel, request generator and MDMA trigger
// (its TC), where the mask write clears that TC, and every flag of the stream there. Lane 3's channel
// copies on lane 1's pacing TC: channels with the same TSEL "are triggered in parallel" and "only the
// channel with the lowest index acknowledges the request" (RM0455 14.5.12), so of each pair the lower
// physical channel clears the TC and the other writes no mask (acknowledges() below). Lane 3 has no
// request generator, DMAMUX synchronization or pacing stream of its own, and a whole block period to
// copy, as the others.
constexpr unsigned Pacers = 3;
constexpr unsigned Pacing[] = {0, 1, 2, 1}; // Whose pacing TC triggers lane i's copy.
DMA_Stream_TypeDef *const pacers[] = {DMA1_Stream2, DMA2_Stream6, DMA1_Stream3};
DMAMUX_Channel_TypeDef *const pacerMux[] = {DMAMUX1_Channel2, DMAMUX1_Channel14, DMAMUX1_Channel3};
DMAMUX_RequestGen_TypeDef *const generators[] = {DMAMUX1_RequestGenerator0, DMAMUX1_RequestGenerator1, DMAMUX1_RequestGenerator2};
constexpr uint32_t triggers[] = {2, 14, 3}; // MDMA TSEL: DMA1 S2, DMA2 S6, DMA1 S3 TC.
volatile uint32_t *const clears[] = {&DMA1->LIFCR, &DMA2->HIFCR, &DMA1->LIFCR};
volatile uint32_t *const statuses[] = {&DMA1->LISR, &DMA2->HISR, &DMA1->LISR};
constexpr uint32_t shifts[] = {16, 16, 22};
constexpr uint32_t Complete = 0x20U, Flags = 0x3dU;
constexpr uint32_t GeneratorMask = (1U << Pacers) - 1; // RGSR/RGCFR bits of the three request generators.
// DMAMUX1 request generator signals: generator 0 follows the channel 1 (lane 0) event, generators 1-2
// the channel 2 event, which lane 0's pacing stream raises once per block: 6 pacing transfers a block.
// On DMA1 those of lanes 0 and 2 follow one another; DMA2 has one, lane 1's.
constexpr uint32_t GeneratorEvents[] = {1, 2, 2};
// Where every pacing transfer writes: PAR of DMA1 Stream7, a stream ScopeHistory owns and never
// enables. PAR is a full 32-bit address that may be written while EN = 0 (RM0455 15.5.7), so any
// AHB word the pacer read is a harmless value; it is reached over the DMA peripheral bus, which
// accesses all AHB peripherals (RM0455 2.1.6). Writes go over AHB1 rather than to an APB timer.
DMA_Stream_TypeDef *const Sink = DMA1_Stream7;
constexpr uint32_t Lane0Request = 23;  // DMAMUX1 request of TIM3 CH1, as CaptureDma programs channel 1.
// The copy's register contract, RM0455 14.5.6 (MDMA_CxTCR), both sides on the system/AXI bus (CTBR
// SBUS = DBUS = 0: the AHB SRAM ring through the AXI interconnect, the AXI SRAM history):
// - source: words (SSIZE = SINCOS = 32 bits: the ring is not read 64 bits wide), bursts of 8 beats;
// - destination: double words (DSIZE = DINCOS = 64 bits, the AXI SRAM width), bursts of 4 beats; PKE
//   packs two source words into one in little-endian order, byte for byte the same history as a
//   32-bit copy (tests/native/mdma_copy);
// - 128-byte buffers (TLEN 127, at most one 128-byte FIFO half): two per 256-byte block, the MDMA
//   arbitrates between the lanes after each. A ramp part of 64 or 192 bytes ends with the shorter
//   64-byte buffer the RM allows when BNDT is not a multiple of TLEN + 1 (14.3.4, 14.5.7);
// - every burst is 32 bytes: within the 128-byte limit and lower than every buffer (TLEN + 1 and the
//   64-byte short one), each buffer a whole number of bursts, every block and address a multiple of
//   8 bytes (BNDT, BRUR, SAR, DAR, the history base).
// One block per trigger (TRGM block), bufferable writes to AXI SRAM. The C8 contract (0x90FC9AAA) moved
// words in 2-beat (8-byte) bursts and 64-byte buffers, although its comment said INCR4.
constexpr uint32_t Transfer = (2U << MDMA_CTCR_SINC_Pos) | (2U << MDMA_CTCR_DINC_Pos) | (2U << MDMA_CTCR_SSIZE_Pos) |
                              (3U << MDMA_CTCR_DSIZE_Pos) | (2U << MDMA_CTCR_SINCOS_Pos) | (3U << MDMA_CTCR_DINCOS_Pos) |
                              (3U << MDMA_CTCR_SBURST_Pos) | (2U << MDMA_CTCR_DBURST_Pos) | (127U << MDMA_CTCR_TLEN_Pos) |
                              MDMA_CTCR_PKE | (1U << MDMA_CTCR_TRGM_Pos) | MDMA_CTCR_BWM;
static_assert(Transfer == 0x93FD3EEAU, "the MDMA copy contract read back on the ZT703S");
constexpr uint32_t Priority = 2U << MDMA_CCR_PL_Pos; // High: below nothing else on the MDMA, bounded by the pacing.

// The lane whose channel has the lowest index among those one pacing TC triggers acknowledges it
// (RM0455 14.5.12): lane 3's channel 0 for lane 1's TC, which lane 1's channel 1 shares.
constexpr bool acknowledges(unsigned lane) {
    for (unsigned other = 0; other < 4; ++other)
        if (other != lane && Pacing[other] == Pacing[lane] && ChannelIndex[other] < ChannelIndex[lane]) return false;
    return true;
}
static_assert(acknowledges(0) && !acknowledges(1) && acknowledges(2) && acknowledges(3), "one acknowledging channel per pacing TC");
Node program(const CaptureBuffer &b, unsigned lane, const HistorySchedule::Node &n) {
    const uint32_t block = b.copyTransfers * 2;
    Node out{};
    out.tcr = Transfer;
    // A ramp node moves `bytes` of each block and skips the rest: the block-repeat update adds the
    // difference to the address after each block (RM0455 14.5.10), so the next block starts on its own.
    out.bndtr = n.bytes | (n.blocks - 1) << MDMA_CBNDTR_BRC_Pos;
    out.brur = (block - n.bytes) << MDMA_CBRUR_SUV_Pos | (block - n.bytes) << MDMA_CBRUR_DUV_Pos;
    out.sar = uint32_t(reinterpret_cast<uintptr_t>(b.lanes[lane])) + n.ringBlock * block;
    out.dar = uint32_t(reinterpret_cast<uintptr_t>(b.history[lane])) + n.historyBlock * block;
    out.lar = uint32_t(reinterpret_cast<uintptr_t>(&nodes[lane][n.next]));
    out.tbr = triggers[Pacing[lane]] << MDMA_CTBR_TSEL_Pos;
    out.mar = acknowledges(lane) ? uint32_t(reinterpret_cast<uintptr_t>(clears[Pacing[lane]])) : 0; // 0: no mask write.
    out.mdr = acknowledges(lane) ? Complete << shifts[Pacing[lane]] : 0;
    return out;
}
bool pending(unsigned pacer) { return (*statuses[pacer] >> shifts[pacer]) & Complete; }
} // namespace

void HistoryCopy::arm(const CaptureBuffer &buffer) {
    buffer_ = buffer;
    __HAL_RCC_MDMA_CLK_ENABLE();
    Sink->CR = 0; // Never enabled: its PAR only takes the pacing writes.
    const unsigned count = HistorySchedule::nodes(buffer);
    for (unsigned lane = 0; lane < 4; ++lane) {
        for (unsigned index = 0; index < count && index < MaximumNodes; ++index)
            nodes[lane][index] = program(buffer, lane, HistorySchedule::node(buffer, index));
        const Node first = program(buffer, lane, HistorySchedule::first(buffer));
        auto *channel = channels[lane];
        channel->CCR = 0;
        channel->CIFCR = 0x1fU;
        channel->CTCR = first.tcr; channel->CBNDTR = first.bndtr;
        channel->CSAR = first.sar; channel->CDAR = first.dar; channel->CBRUR = first.brur;
        channel->CLAR = first.lar; channel->CTBR = first.tbr;
        channel->CMAR = first.mar; channel->CMDR = first.mdr;
    }
    for (unsigned lane = 0; lane < Pacers; ++lane) { // The pacing streams of lanes 0-2 (lane 3 uses lane 1's).
        auto *pacer = pacers[lane];
        pacer->CR = 0;
        *clears[lane] = Flags << shifts[lane];
        pacer->PAR = uint32_t(reinterpret_cast<uintptr_t>(&Sink->PAR));
        pacer->M0AR = uint32_t(reinterpret_cast<uintptr_t>(buffer.lanes[0])); // Read only, preloaded (RM0455 15.3.8).
        const uint32_t perBlock = buffer.copyTransfers / HistorySchedule::EventRequests;
        pacer->NDTR = lane ? 1 : perBlock; // Lane 0's stream counts the events of a block, the others one each.
        pacer->FCR = 0; // Direct: each request moves its word at once.
        // Very high, as the NOR streams: below the capture lanes a stream starves at 140 MS/s and the
        // request generator would overrun; at most one pacing transfer at a time per controller. No IRQ.
        pacer->CR = DMA_SxCR_DIR_0 | DMA_SxCR_CIRC | DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1 | DMA_SxCR_PL;
        pacerMux[lane]->CCR = 1U + lane; // Request generator `lane`.
        // Lane 0's stream raises the block event for the other generators (NBREQ only while EGE is clear).
        if (!lane) pacerMux[lane]->CCR = 1U | (perBlock - 1) << DMAMUX_CxCR_NBREQ_Pos | DMAMUX_CxCR_EGE;
        generators[lane]->RGCR = 0;
        generators[lane]->RGCR = GeneratorEvents[lane] << DMAMUX_RGxCR_SIG_ID_Pos | 1U << DMAMUX_RGxCR_GPOL_Pos; // Rising, one request.
    }
    DMAMUX1_RequestGenStatus->RGCFR = GeneratorMask;
    __DSB(); // The nodes are in non-cacheable AXI SRAM; the MDMA reads them after this.
    for (unsigned lane = 0; lane < 4; ++lane) channels[lane]->CCR = Priority | MDMA_CCR_TEIE | MDMA_CCR_EN;
    for (unsigned pacer = 0; pacer < Pacers; ++pacer) {
        pacers[pacer]->CR |= DMA_SxCR_EN;
        generators[pacer]->RGCR |= DMAMUX_RGxCR_GE;
    }
    // NBREQ may be written only while EGE is clear; the count starts with lane 0's first request.
    DMAMUX1_Channel1->CCR = Lane0Request;
    DMAMUX1_Channel1->CCR = Lane0Request | (HistorySchedule::EventRequests - 1) << DMAMUX_CxCR_NBREQ_Pos | DMAMUX_CxCR_EGE;
    __DSB();
    armed_ = true;
}
bool HistoryCopy::finish(uint32_t (&served)[4]) {
    if (!armed_) return false;
    // No new trigger: the lanes are stopped and now the event and the generators are too. A request
    // a generator already made still completes its pacing transfer and may still raise a TC.
    DMAMUX1_Channel1->CCR &= DMAMUX_CxCR_DMAREQ_ID_Msk;
    for (auto *generator : generators) generator->RGCR = 0;
    __DSB();
    bool busy = true; // At most a block per lane is left: microseconds; this runs in the freeze ISR.
    for (unsigned wait = 0; wait < 2000 && busy; ++wait) {
        busy = false;
        for (unsigned lane = 0; lane < 4; ++lane)
            busy |= pending(Pacing[lane]) || (channels[lane]->CISR & MDMA_CISR_CRQA);
    }
    uint32_t error = 0;
    // A trigger came while one was still waiting (generator): a copy may be missing.
    const bool overrun = DMAMUX1_RequestGenStatus->RGSR & GeneratorMask;
    // The phase is read only from stopped channels: whatever TC follows, they copy nothing more.
    for (auto *channel : channels) channel->CCR &= ~MDMA_CCR_EN;
    bool stopped = true;
    for (auto *channel : channels) {
        unsigned wait = 0;
        while ((channel->CCR & MDMA_CCR_EN) && wait < 10000) ++wait;
        stopped &= !(channel->CCR & MDMA_CCR_EN);
    }
    bool ok = !busy && !overrun && stopped && !failed(error);
    for (unsigned lane = 0; ok && lane < 4; ++lane) {
        const uint32_t offset = channels[lane]->CDAR - uint32_t(reinterpret_cast<uintptr_t>(buffer_.history[lane]));
        ok = HistorySchedule::servedAt(buffer_, offset, served[lane]); // Each lane is checked on its own.
    }
    if (stopped) stop(); // The pacing streams now; their disable TC reaches no enabled channel.
    return ok;
}
bool HistoryCopy::stop() {
    if (!armed_) return true;
    DMAMUX1_Channel1->CCR &= DMAMUX_CxCR_DMAREQ_ID_Msk; // No more events.
    for (auto *generator : generators) generator->RGCR = 0;
    for (auto *channel : channels) channel->CCR &= ~MDMA_CCR_EN;
    bool stopped = true, channelsOff = true;
    for (unsigned lane = 0; lane < 4; ++lane) {
        unsigned wait = 0;
        while ((channels[lane]->CCR & MDMA_CCR_EN) && wait < 10000) ++wait;
        if (channels[lane]->CCR & MDMA_CCR_EN) { stopped = channelsOff = false; continue; }
        channels[lane]->CIFCR = 0x1fU;
    }
    // Only with every MDMA channel off (lane 3's shares lane 1's trigger): the TC that disabling a
    // pacing stream sets copies nothing.
    for (unsigned pacer = 0; channelsOff && pacer < Pacers; ++pacer) {
        if (!DmaStream::disable(pacers[pacer])) { stopped = false; continue; }
        *clears[pacer] = Flags << shifts[pacer];
        pacerMux[pacer]->CCR = 0;
    }
    DMAMUX1_RequestGenStatus->RGCFR = GeneratorMask;
    __DSB();
    armed_ = !stopped;
    return stopped;
}
uint32_t HistoryCopy::source(unsigned lane) const {
    const uint32_t offset = channels[lane]->CSAR - uint32_t(reinterpret_cast<uintptr_t>(buffer_.lanes[lane]));
    return offset / 2 % buffer_.laneTransfers;
}
uint32_t HistoryCopy::lead(unsigned lane, const volatile uint32_t &ndtr) const {
    const uint32_t ring = buffer_.laneTransfers, source = this->source(lane);
    const uint32_t written = (ring - ndtr) % ring; // Read second: the lane only moves forward.
    return (source + ring - written) % ring;
}
uint32_t HistoryCopy::failed(uint32_t &error) {
    uint32_t lanes = 0;
    for (unsigned lane = 0; lane < 4; ++lane)
        if (channels[lane]->CISR & MDMA_CISR_TEIF) {
            if (!lanes) error = channels[lane]->CESR;
            lanes |= 1U << lane;
        }
    return lanes;
}
#ifdef OPENOSC_DIAG_MAILBOX
void HistoryCopy::snapshot(OpenoscFirstFault &r) {
    uint32_t map = 0;
    for (unsigned lane = 0; lane < 4; ++lane) {
        const auto *c = channels[lane];
        r.mdmaCCR[lane] = c->CCR; r.mdmaCISR[lane] = c->CISR; r.mdmaCESR[lane] = c->CESR; r.mdmaCBNDTR[lane] = c->CBNDTR;
        r.mdmaCSAR[lane] = c->CSAR; r.mdmaCDAR[lane] = c->CDAR; r.mdmaCLAR[lane] = c->CLAR;
        map |= ChannelIndex[lane] << (4 * lane);
    }
    r.laneChannels = map;
    for (unsigned pacer = 0; pacer < Pacers; ++pacer) { r.pacerCR[pacer] = pacers[pacer]->CR; r.pacerNDTR[pacer] = pacers[pacer]->NDTR; }
    r.muxChannel1 = DMAMUX1_Channel1->CCR; r.muxChannel2 = DMAMUX1_Channel2->CCR;
    r.muxRGSR = DMAMUX1_RequestGenStatus->RGSR; r.muxCSR = DMAMUX1_ChannelStatus->CSR;
}
#endif
void HistoryCopy::clearInterrupts() {
    for (auto *channel : channels) channel->CIFCR = 0x1fU;
}
} // namespace openosc::scope
