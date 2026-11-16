#include "openosc/measurement/dmm/meter_commands.hpp"

namespace openosc::dmm {
static_assert(KeyCommand::StrokeMs >= RangeStroke::OpenLimitMs + 2 * RangeStroke::StepLimitMs &&
              KeyCommand::StrokeMs >= RangeStroke::HoldGapMs + 3 * RangeStroke::StepLimitMs,
              "a stroke's command outlives the stroke's own bounds");
bool MeterCommands::selectFunction(Function function, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (key_.busy() || range_.busy() || stroke_.active()) return false;
    if (!selector_.request(function, latest, nowMs, minimumUs)) return false;
    operation_ = Operation::Function; return true;
}
bool MeterCommands::requestKey(KeyCommand::Action action, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (selector_.busy() || range_.busy() || stroke_.active()) return false;
    using Action = KeyCommand::Action;
    const bool range = action == Action::NextRange || action == Action::Auto || action == Action::Manual;
    if (range && !Ranges::selectable(Functions::identify(latest))) return false;
    if (!key_.request(action, latest, nowMs, minimumUs)) return false;
    operation_ = Operation::Key; return true;
}
bool MeterCommands::setRange(unsigned index, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (selector_.busy() || key_.busy() || stroke_.active() || latest.timestampUs < minimumUs) return false;
    if (!range_.request(index, latest, nowMs)) return false;
    operation_ = Operation::Range; return true;
}
bool MeterCommands::pressRange(const Result &latest, uint32_t nowMs, uint64_t nowUs, uint64_t minimumUs) {
    const bool fresh = latest.sequence && latest.timestampUs >= minimumUs && nowUs >= latest.timestampUs &&
                       nowUs - latest.timestampUs <= FreshUs;
    if (!fresh || !idle() || !requestKey(KeyCommand::Action::NextRange, latest, nowMs, minimumUs)) return false;
    stroke_.start(RangeStroke::End::Open, nowMs); // Its 20 on the next service.
    return true;
}
bool MeterCommands::holdRange(uint32_t nowMs) {
    if (stroke_.closing() || !stroke_.finish(RangeStroke::End::Hold, nowMs)) return false;
    key_.hold();
    return true;
}
bool MeterCommands::close(uint32_t nowMs) {
    selector_.cancel(); key_.cancel(); range_.cancel();
    stroke_.close(nowMs);
    return !stroke_.active();
}
void MeterCommands::abandon() {
    selector_.cancel(); key_.cancel(); range_.cancel(); stroke_.abandon();
}
void MeterCommands::cancelOwner() {
    if (operation_ == Operation::Key) key_.cancel();
    else if (operation_ == Operation::Range) range_.cancel();
}
// NextRange: a short stroke (20, 21); AutoRange: a hold (20, 22). Other commands are not strokes.
bool MeterCommands::startStroke(CommandGroup command, uint32_t nowMs) {
    using End = RangeStroke::End;
    if (command != CommandGroup::NextRange && command != CommandGroup::AutoRange) return false;
    return stroke_.start(command == CommandGroup::AutoRange ? End::Hold : End::Release, nowMs);
}
void MeterCommands::service(CommandPort &port, const Result &latest, uint32_t nowMs) {
    if (stroke_.active()) { serviceStroke(port, latest, nowMs); return; } // Nothing else until its end left.
    CommandGroup command;
    if (selector_.poll(latest, nowMs, command) && port.txStatus() != DataStatus::Busy) {
        if (port.send(command, nowMs)) selector_.sent(latest.sequence, nowMs);
        else selector_.fail();
    }
    if (key_.poll(latest, nowMs, command) && !startStroke(command, nowMs) && port.txStatus() != DataStatus::Busy) {
        if (port.send(command, nowMs)) key_.sent(latest.sequence, nowMs); // HOLD release, REL off.
        else key_.fail();
    }
    if (!stroke_.active() && range_.poll(latest, nowMs, command)) startStroke(command, nowMs);
    if (stroke_.active()) serviceStroke(port, latest, nowMs); // Its 20 at once, when the UART is free.
}
void MeterCommands::serviceStroke(CommandPort &port, const Result &latest, uint32_t nowMs) {
    using Event = RangeStroke::Event;
    const bool closing = stroke_.closing(); // Its last event (Done, Failed) comes once it is idle.
    auto event = stroke_.service(port.txStatus(), nowMs);
    CommandGroup command;
    if (event == Event::None && port.txStatus() != DataStatus::Busy && stroke_.due(nowMs, command))
        event = stroke_.accepted(port.send(command, nowMs), nowMs);
    // A closing stroke (close, the open limit) belongs to no command: cancelled, never confirmed.
    if (closing || stroke_.closing()) { cancelOwner(); return; }
    const bool key = operation_ == Operation::Key;
    if (event == Event::PressSent && key) key_.pressed(latest, nowMs); // RangeSelector follows packets itself.
    if (event == Event::EndSent) {
        if (key) key_.sent(latest.sequence, nowMs);
        else range_.sent(latest.sequence, nowMs);
    }
    if (event == Event::Failed) {
        if (key) key_.fail();
        else range_.fail();
    }
}
} // namespace openosc::dmm
