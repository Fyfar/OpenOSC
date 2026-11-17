#pragma once
#include "openosc/measurement/dmm/function.hpp"
#include "openosc/measurement/dmm/key_command.hpp"
#include "openosc/measurement/dmm/range_stroke.hpp"
#include "openosc/measurement/dmm/ranges.hpp"

namespace openosc::dmm {
// The controller's command UART as the commands see it (UartDma; a scripted port in tests).
class CommandPort {
public:
    virtual bool send(CommandGroup group, uint32_t nowMs) = 0; // Accepted: txStatus Busy until it left.
    virtual DataStatus txStatus() const = 0;
protected:
    ~CommandPort() = default;
};
// Every DMM command on the UART, one operation at a time: the function selector (F keys, SCPI, the mA
// restore), the controller keys (KeyCommand: AUTO/RANGE, HOLD release, REL off) and the range selector,
// AUTO/RANGE always as one RangeStroke. While a stroke runs nothing else is sent and nothing new is
// accepted; the next command goes only once the stroke's end has left the UART. Requests refused here
// change nothing. `minimumUs`: the DMM's activation (a packet older than it is not this session's).
class MeterCommands {
public:
    // A physical press needs a reading of this session no older than this (a blind 20 is never sent).
    static constexpr uint64_t FreshUs = 2000000;
    bool selectFunction(Function function, const Result &latest, uint32_t nowMs, uint64_t minimumUs);
    bool requestKey(KeyCommand::Action action, const Result &latest, uint32_t nowMs, uint64_t minimumUs);
    bool setRange(unsigned index, const Result &latest, uint32_t nowMs, uint64_t minimumUs);
    // The physical AUTO/RANGE (KeyGestures press, release/releaseLate, holdRepeat): the qualified press
    // opens a NextRange stroke (its 20); the release within the factory's window ends it with 21, a later
    // one with nothing (the command is then judged from the release); each hold sends a 22 (the first
    // makes the command Auto). A press while the previous physical stroke's command only awaits its
    // packet (nothing owed on the wire) supersedes it: that command ends without a status.
    bool pressRange(const Result &latest, uint32_t nowMs, uint64_t nowUs, uint64_t minimumUs);
    bool releaseRange(bool sendRelease, uint32_t nowMs) { return stroke_.release(sendRelease, nowMs); }
    bool holdRange(uint32_t nowMs);
    bool rangeOpen() const { return stroke_.open() && !stroke_.closing(); }
    void service(CommandPort &port, const Result &latest, uint32_t nowMs);
    // Controlled cancel: every command is cancelled (none is confirmed later); a stroke whose 20 went
    // still sends its 21 (RangeStroke::close). True once nothing is owed on the wire.
    bool close(uint32_t nowMs);
    bool owes() const { return stroke_.active(); }
    // The UART stopped or failed: nothing more can be sent; everything ends without a status.
    void abandon();
    bool busy() const { return selector_.busy() || key_.busy() || range_.busy() || stroke_.active(); }
    DataStatus status() const {
        return operation_ == Operation::Key ? key_.status() : operation_ == Operation::Range ? range_.status() : selector_.status();
    }
    DataStatus selectionStatus() const { return selector_.status(); }
private:
    enum class Operation : uint8_t { Function, Key, Range };
    bool idle() const { return !busy(); }
    bool startStroke(CommandGroup command, uint32_t nowMs);
    void serviceStroke(CommandPort &port, const Result &latest, uint32_t nowMs);
    void cancelOwner();
    FunctionSelector selector_;
    KeyCommand key_;
    RangeSelector range_;
    RangeStroke stroke_; // Owned by the Key or Range operation that started it.
    Operation operation_ = Operation::Function;
    bool physical_ = false; // The Key operation is the physical AUTO/RANGE's.
    bool ended_ = false;    // The running stroke's end went (EndSent, Released).
};
} // namespace openosc::dmm
