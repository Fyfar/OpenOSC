#pragma once
// Diagnostic builds only (env firmware_diag, OPENOSC_DIAG_MAILBOX; from candidate17); the normal firmware
// does not contain it. It runs the one local 60 s 64K -> 300 s 512K scope test without USB: the host writes one
// command word over SWD and the main loop serves it through the instrument's ordinary backend (the SCPI
// engines' interface: configureScope, run, stop, frame, releaseFrame, performance, the generator). The
// normal DataAccess, capture, FrameStore, leases, UI and waveform stay in charge. Five fixed commands,
// no general remote procedure call.
#include "openosc/scpi/backend.hpp"
#include "openosc/update/crc32.hpp"
#include <cstddef>
#include <cstdint>

namespace openosc::diag {
// ABI version 2 of `openosc_diag_mailbox`: 74 little-endian 32-bit words in DTCM (.bss, zero at boot).
// The offsets are fixed by the static_asserts below; the host child mirrors them and first checks the
// symbol's address and size in this image's ELF, then magic, version and size, before its first write.
// - command: written only by the host, sequence (1..255) << 8 | code. A new sequence is served once.
// - telemetry: 10 words, one record. The device rewrites it whole (on a command, a phase change, while a
//   stop completes, and at most every ReportMs otherwise): `begin` (the highest word) first, then the
//   fields, then `end` (the lowest word), both the same new count. The host reads the 10 words in one
//   increasing-address SWD block read and takes them only when end == begin: a read that overlaps a
//   rewrite sees end (old) != begin (new). It is the only part read while a stage runs (with the
//   capture-fault counter). The heartbeat is the main-loop pass count at the record's time.
// - config, descriptor: each behind its own publish word (odd while it is written, then even and
//   larger) and closed by a CRC32 of its other words. Written once per start / once per described
//   stop; the host reads them in HOLD only.
enum class Code : uint8_t { None, Stop, AckStopped, AckDone, Start64, Start512 };
enum class Phase : uint8_t { Idle, Holding, Held, Describing, Described, Released, Running, Done, Failed };
enum class Reason : uint8_t {
    None, WrongPhase, Order, NotHeld, Clock, Channels, Calibration, Generator, Configure, Plan, Run,
    HoldTimeout, NoFrame, Stale, UnexpectedRun, UnexpectedHold, CaptureStatus, UnknownCommand
};
struct Telemetry { uint32_t end, state, heartbeat, uptimeMs, stageMs, captures, projections, flushes, sequence, begin; };
struct Config {
    uint32_t stage, coreHz, budget, planSamples, rateNumerator, rateDenominator, timebaseNs, channels, millivolts[2],
             probes, trigger, generator, generatorNumerator, generatorDenominator, startSequence, startCaptures,
             durationMs, check;
};
struct Descriptor {
    uint32_t frameValid, sequence, samples, ring, origin, layout, rateNumerator, rateDenominator,
             timestampLow, timestampHigh, timebaseLow, timebaseHigh, flags, triggerIndex, banks[4],
             ahbWords, historyWords, older[4], end[4], newest[4], crc[2],
             capturesAtStop, msSinceCapture, stopUptimeMs, stageMs, check;
};
struct Mailbox {
    uint32_t magic, version, size, command;
    Telemetry telemetry;
    uint32_t configPublish;
    Config config;
    uint32_t descriptorPublish;
    Descriptor descriptor;
};
constexpr uint32_t Magic = 0x4C373143U, Version = 2; // "C17L"
static_assert(sizeof(Telemetry) == 10 * 4 && sizeof(Config) == 19 * 4 && sizeof(Descriptor) == 39 * 4 &&
              sizeof(Mailbox) == 74 * 4, "mailbox ABI v2 size");
static_assert(offsetof(Mailbox, command) == 12 && offsetof(Mailbox, telemetry) == 16 && offsetof(Mailbox, configPublish) == 56 &&
              offsetof(Mailbox, config) == 60 && offsetof(Mailbox, descriptorPublish) == 136 &&
              offsetof(Mailbox, descriptor) == 140, "mailbox ABI v2 offsets");
static_assert(offsetof(Telemetry, end) == 0 && offsetof(Telemetry, state) == 4 && offsetof(Telemetry, begin) == 9 * 4 &&
              offsetof(Config, check) == 18 * 4 && offsetof(Descriptor, banks) == 14 * 4 && offsetof(Descriptor, older) == 20 * 4 &&
              offsetof(Descriptor, crc) == 32 * 4 && offsetof(Descriptor, check) == 38 * 4, "mailbox ABI v2 fields");
// telemetry.state: phase bits 0-3, stage 4-5, lease held 6, acknowledged sequence 8-15, result of that
// command (or the failure reason) 16-23, capture status 24-30, capture active 31.
constexpr uint32_t packState(Phase phase, unsigned stage, bool lease, uint8_t ack, Reason reason, DataStatus status, bool active) {
    return uint32_t(phase) | (stage & 3U) << 4 | uint32_t(lease) << 6 | uint32_t(ack) << 8 | uint32_t(reason) << 16 |
           (uint32_t(status) & 0x7fU) << 24 | uint32_t(active) << 31;
}

// Serves the mailbox from the main loop. Stage 1 is 64K, stage 2 512K (two fast channels plan the split
// ring by default; the host still proves the physical stop before it starts the stage). Each stage:
//   Released --Start--> Running --own timer--> Holding --held, capture inactive, record leased--> Held
//   --AckStopped (the host proved the physical stop)--> Describing (CRC32 of both channels through the
//   published FrameView) --> Described --AckDone (the host has its dump)--> Released (or Done).
// The boot capture is first stopped by the host's Stop, through the same Holding/Held/AckStopped/AckDone.
// - Starts are served only in Released, in stage order, with the scope logically held: never over a
//   running capture. A Start in another phase is refused and changes nothing.
// - Any check that fails after the instrument was touched, a capture fault status, an unexpected RUN or
//   HOLD, a stale record or a hold that does not complete ends in Failed: the ordinary HOLD is requested
//   if the scope is not held (backend.stop(); whether the capture physically stopped is the host's proof,
//   never claimed here), the lease on the held record stays, no further start. Only Stop is still served.
// - Each pass first checks that the scope is still held in the phases that need it, before it serves a
//   command, gives back a lease or reads a sample: a foreign RUN ends in Failed before any of those.
class LocalControl {
public:
    struct Stage { uint32_t requestPoints, planSamples, durationMs; };
    // Per channel, two channels at 140 MS/s. The device runs each stage a little longer than the host's
    // 60 / 300 s so that the host observes the full time at its one-second polling.
    static constexpr Stage Stages[3] = {{0, 0, 0}, {32768, 32768, 65000}, {262144, 258592, 305000}};
    static constexpr uint32_t CoreHz = 280000000, Rate = 140000000, TimebaseNs = 5000;
    static constexpr uint32_t ReportMs = 100, HoldTimeoutMs = 3000, CrcChunk = 4096;
    static constexpr double TriggerVolts = 1.5;
    static constexpr unsigned ChannelMillivolts = 2000;

    explicit LocalControl(scpi::Backend &backend);
    void poll(uint32_t now);
    Phase phase() const { return phase_; }

private:
    void serve(uint32_t command, uint32_t now);
    Reason start(unsigned stage, uint32_t now, const PerformanceSnapshot &performance);
    Reason channels() const;
    Reason generator();
    bool generatorReady() const;
    void running(uint32_t now, const PerformanceSnapshot &performance, const scope::CaptureProgress &progress);
    void stopStage(uint32_t now);
    void holding(uint32_t now, const scope::CaptureProgress &progress);
    void describe();
    void described();
    void watch();
    void fail(Reason reason);
    void report(uint32_t now, const PerformanceSnapshot &performance, const scope::CaptureProgress &progress);

    scpi::Backend &backend_;
    Phase phase_ = Phase::Idle;
    unsigned stage_ = 0, completed_ = 0;
    bool aborted_ = false;
    uint8_t ack_ = 0;
    Reason result_ = Reason::None;
    uint32_t heartbeat_ = 0, reportAt_ = 0, runAt_ = 0, holdAt_ = 0, stageMs_ = 0;
    uint32_t captures_ = 0, captureAt_ = 0, capturesAtStop_ = 0, msSinceCapture_ = 0, stopAt_ = 0;
    uint32_t startSequence_ = 0, configPublish_ = 0, descriptorPublish_ = 0, telemetryCount_ = 0;
    const scope::FrameView *view_ = nullptr;
    uint32_t index_ = 0;
    Crc32 crc_[2];
};
} // namespace openosc::diag

extern "C" volatile openosc::diag::Mailbox openosc_diag_mailbox;
