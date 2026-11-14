#pragma once
#include <cstdint>

// Diagnostic ring buffer for USB bus events; read back over SWD. Without
// OPENOSC_USB_TRACE every hook below is an empty inline function.
namespace openosc::usbtrace {
enum class Tag : uint8_t {
    Irq = 1,        // value: masked GINTSTS reset/suspend/wakeup bits
    IrqRaw,         // value: raw GINTSTS at the same moment
    Out,            // value: endpoint | byte count << 8
    Command,        // value: first four header characters
    ConfigBegin,    // value: 1 when the request is unchanged
    ConfigEnd,
    FrontendBegin,
    FrontendEnd,
    LatchBegin,
    LatchEnd,
    SaveBegin,
    SaveEnd,
    Nor,            // value: opcode | address << 8
    LineBegin,      // value: D-, D+, USB33RDY sample that stayed away from idle
    LineEnd,        // value: the same sample | sample count << 8
    CaptureBegin,
    CaptureEnd,
    Latched,        // frontend latch written
    Offset,         // TIM8 offset compare written
    Reference,      // PA4 / DAC1 reference done
    Foreground,     // value: address interrupted by a USB reset event
    ForegroundLr,   // value: its return address
    ForegroundR0,   // value: its first argument register
    ForegroundR2,   // value: its third argument register
    Power,          // value: PWR_CR3 at a bus reset
    Control,        // value: OTG DCTL at a bus reset
    Session,        // value: OTG GOTGCTL at a bus reset
    AdcStart,       // the external ADC clock has just been started
    AdcStop,        // the external ADC clock has just been stopped
    CaptureFault,   // value: bits 0-5 DMA stream flags (FE, -, DME, TE, HT, TC) of lane bits 8-9; bit 16 a stream
                    // that would not disable in quiesce, bit 17 at the freeze (bits 0-5 FCR), bit 18 gate prepare,
                    // bit 21 a lane TC late or a missed segment switch (bits 0-1 which, 12-19 cycles), bit 22
                    // a frozen window refused (bits 0-21 its size): both end the capture with Overflow.
                    // Split ring (HistoryCopy): bit 19 its copies out of step at the freeze (bits 0-8 the
                    // triggers served, 23-31 those lane 0 made, modulo the cycle; 0x1ff unknown), bit 20 an
                    // MDMA channel or pacing stream that would not stop, bit 23 an MDMA transfer error
                    // (bits 0-3 lanes, 4-15 CESR of the first)
#ifdef OPENOSC_USB_TRACE
    PowerTransition, // value: PWR_CR3 on every sampled USB33RDY transition
    Reattach,        // value: bus-reset count before software watchdog disconnect
    PowerInputs,     // value: raw GPIOC bits, external-power and board-specific charge status
#endif
    // After the trace-only tags, which keep their numbers in existing dumps.
    CaptureStop,    // value: points the freeze landed after the triggered record's end (bit 31: before it),
                    // bits 16-30 the ring points left before the record
    CaptureLead,    // value: a split ring's copy lead over its lane (HistoryCopy::lead) when it is that lane's new
                    // smallest or out of range: bits 0-15 words, 16-17 lane, bit 18 out of range (0 or past
                    // Lead + 1 blocks: a copy behind its lane), bits 19-31 the poll samples before it
    CaptureCopies,  // value: at each split freeze, lane 0's phase in its block (bits 0-7) and lanes 1-3's triggers
                    // served behind lane 0's, modulo 256 (bits 8-15, 16-23, 24-31): 0 in every published frame,
                    // lane 3 copies on lane 1's trigger
};
#ifdef OPENOSC_USB_TRACE
void start();
void mark(Tag tag, uint32_t value = 0);
void irq(uint32_t masked, uint32_t raw, const uint32_t *frame);
#else
inline void start() {}
inline void mark(Tag, uint32_t = 0) {}
#endif
} // namespace openosc::usbtrace
