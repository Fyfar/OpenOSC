#include "openosc/generator/module_link.hpp"

namespace openosc::generator {
void ModuleLink::queued() {
    // A frame waiting for an acknowledge is superseded: its answer no longer decides anything.
    if (state_ == State::Idle || state_ == State::Acknowledging) state_ = State::Waiting;
}
void ModuleLink::submit(const ModuleWave::Frame &frame) { wave_ = frame; wavePending_ = true; queued(); }
void ModuleLink::submitOff(const ModuleWave::Frame &frame) {
    off_ = frame; offPending_ = true; wavePending_ = false; queued();
}
const ModuleWave::Frame &ModuleLink::begin(uint32_t now) {
    if (offPending_) { current_ = off_; currentOff_ = true; offPending_ = false; attempts_ = 0; }
    else if (wavePending_) { current_ = wave_; currentOff_ = false; wavePending_ = false; attempts_ = 0; }
    ++attempts_;
    state_ = State::Sending; deadline_ = now + TransmitMs;
    return current_;
}
void ModuleLink::sent(uint32_t now) {
    if (state_ != State::Sending) return;
    state_ = State::Acknowledging; deadline_ = now + AcknowledgeMs;
}
bool ModuleLink::received(uint8_t byte) {
    if (state_ != State::Acknowledging || byte != current_.acknowledge) return false;
    status_ = DataStatus::Ok;
    state_ = pending() ? State::Waiting : State::Idle;
    return true;
}
void ModuleLink::interrupt() {
    if (state_ != State::Acknowledging) return;
    if (attempts_) --attempts_;
    state_ = State::Waiting;
}
ModuleLink::Failure ModuleLink::service(uint32_t now) {
    if ((state_ != State::Sending && state_ != State::Acknowledging) || int32_t(now - deadline_) < 0) return Failure::None;
    if (state_ == State::Sending) { // The UART did not finish: nothing more over a stuck transmitter.
        state_ = State::Idle; status_ = DataStatus::HardwareError; offPending_ = wavePending_ = false;
        return Failure::Stuck;
    }
    if (pending() || attempts_ < Attempts) { state_ = State::Waiting; return Failure::None; }
    state_ = State::Idle; status_ = DataStatus::Timeout;
    return Failure::Timeout;
}
} // namespace openosc::generator
