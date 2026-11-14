#pragma once
#include "openosc/generator/module_wave.hpp"
#include "openosc/data/status.hpp"

namespace openosc::generator {
// The frame exchange with the ZT706S module, without the UART (ModuleOutput drives the bytes): one
// 512-byte frame on the wire at a time (~0.53 s at 9600 8N1), never cut short (whether the module
// resynchronises on a partial frame is unknown). Waiting behind it: an OFF frame, then the newest ON
// frame submitted after it; an OFF drops the ON frames submitted before it.
// Accepted when the folded checksum byte of the frame comes back after its last stop bit, the byte
// the factory main waits for (706S main 08036CDC; the factory GEN takes any received byte, 24071D0C,
// and the main retries without limit). Here: AcknowledgeMs per attempt, Attempts attempts, then
// Timeout. The peer and its real answer are not established on hardware: an acknowledge means only
// that the matching byte came back.
class ModuleLink {
public:
    static constexpr unsigned Attempts = 3;
    static constexpr uint32_t TransmitMs = 1100;     // Twice the frame's wire time: beyond it the UART is stuck.
    static constexpr uint32_t AcknowledgeMs = 300;
    // Bound of one exchange from submit: a frame already on the wire, then Attempts attempts.
    static constexpr uint32_t ExchangeMs = TransmitMs + Attempts * (TransmitMs + AcknowledgeMs);
    enum class State : uint8_t { Idle, Waiting, Sending, Acknowledging };
    enum class Failure : uint8_t { None, Timeout, Stuck }; // Stuck: the UART must be stopped.
    void submit(const ModuleWave::Frame &frame);     // ON: after a waiting OFF.
    void submitOff(const ModuleWave::Frame &frame);  // OFF: before every older ON, which it drops.
    bool due() const { return state_ == State::Waiting; }
    const ModuleWave::Frame &begin(uint32_t now);    // The frame to put on the wire now (due() first).
    void sent(uint32_t now);                         // Its last stop bit is out.
    // A byte received after the last stop bit of the frame now waiting for its acknowledge (the
    // caller drops older bytes and bytes with UART errors). True when it acknowledges it.
    bool received(uint8_t byte);
    // A run-mode change may stop the UART only between frames: not while one is on the wire.
    bool suspendable() const { return state_ != State::Sending; }
    // The UART stopped between frames: a frame waiting for its acknowledge goes again whole, not
    // counted as an attempt (its answer could be lost while the UART was off).
    void interrupt();
    Failure service(uint32_t now);                   // Deadlines.
    bool busy() const { return state_ != State::Idle; }
    bool offQueued() const { return offPending_ || (busy() && currentOff_); }
    bool currentOff() const { return currentOff_; }
    State state() const { return state_; }
    DataStatus status() const { return busy() ? DataStatus::Busy : status_; }
    unsigned attempts() const { return attempts_; }

private:
    bool pending() const { return offPending_ || wavePending_; }
    void queued();
    ModuleWave::Frame current_, off_, wave_;
    State state_ = State::Idle;
    DataStatus status_ = DataStatus::NoData;
    uint32_t deadline_ = 0;
    unsigned attempts_ = 0;
    bool offPending_ = false, wavePending_ = false, currentOff_ = false;
};
} // namespace openosc::generator
