#include "openosc/measurement/dmm/meter_commands.hpp"

namespace openosc::dmm {
static_assert(KeyCommand::StrokeMs >= RangeStroke::QuietLimitMs + 2 * RangeStroke::StepLimitMs &&
              KeyCommand::StrokeMs >= RangeStroke::HoldGapMs + 3 * RangeStroke::StepLimitMs,
              "a stroke's command outlives the stroke's own bounds");
bool MeterCommands::selectFunction(Function function, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (key_.busy() || range_.busy() || stroke_.active() || group_ == DataStatus::Busy || !fresh(latest)) return false;
    if (!selector_.request(function, latest, nowMs, minimumUs)) return false;
    operation_ = Operation::Function; groupChosen_ = false; return true;
}
bool MeterCommands::pressGroup(CommandGroup group, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (busy() || !latest.sequence || latest.timestampUs < minimumUs) return false;
    groupKey_ = group; group_ = DataStatus::Busy; groupSent_ = false; groupDeadline_ = nowMs + GroupMs;
    operation_ = Operation::Group; groupChosen_ = true; return true;
}
void MeterCommands::serviceGroup(CommandPort &port, const Result &latest, uint32_t nowMs) {
    const auto tx = port.txStatus();
    const bool ended = groupSent_ && tx != DataStatus::Busy; // The pair's own end: left, or the UART's transmit timeout.
    if (ended || int32_t(nowMs - groupDeadline_) >= 0) {
        group_ = !ended ? DataStatus::Timeout : tx == DataStatus::Ok || tx == DataStatus::Timeout ? tx : DataStatus::HardwareError;
        if (groupSent_) { fenced_ = true; fence_ = latest.sequence; } // Even a failed pair may have reached it.
        return;
    }
    if (groupSent_ || tx == DataStatus::Busy) return; // A pair before it still leaving: sent after it.
    if (port.send(groupKey_, nowMs)) groupSent_ = true;
    else group_ = DataStatus::HardwareError;
}
bool MeterCommands::requestKey(KeyCommand::Action action, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (selector_.busy() || range_.busy() || stroke_.active() || group_ == DataStatus::Busy || !fresh(latest)) return false;
    using Action = KeyCommand::Action;
    const bool range = action == Action::NextRange || action == Action::Auto || action == Action::Manual;
    if (range && !Ranges::selectable(Functions::identify(latest))) return false;
    if (!key_.request(action, latest, nowMs, minimumUs)) return false;
    operation_ = Operation::Key; physical_ = false; return true;
}
bool MeterCommands::setRange(unsigned index, const Result &latest, uint32_t nowMs, uint64_t minimumUs) {
    if (selector_.busy() || key_.busy() || stroke_.active() || group_ == DataStatus::Busy || !fresh(latest) ||
        latest.timestampUs < minimumUs) return false;
    if (!range_.request(index, latest, nowMs)) return false;
    operation_ = Operation::Range; return true;
}
bool MeterCommands::pressRange(const Result &latest, uint32_t nowMs, uint64_t nowUs, uint64_t minimumUs) {
    const bool recent = latest.sequence && latest.timestampUs >= minimumUs && nowUs >= latest.timestampUs &&
                        nowUs - latest.timestampUs <= FreshUs;
    if (!recent || !fresh(latest) || !Ranges::selectable(Functions::identify(latest))) return false;
    if (operation_ == Operation::Key && physical_ && key_.busy() && !stroke_.active()) key_.cancel(); // Superseded.
    if (!idle() || !requestKey(KeyCommand::Action::NextRange, latest, nowMs, minimumUs)) return false;
    physical_ = true; ended_ = false;
    stroke_.start(RangeStroke::End::Open, nowMs); // Its 20 on the next service.
    return true;
}
bool MeterCommands::holdRange(uint32_t nowMs) {
    if (!stroke_.hold(nowMs)) return false;
    key_.hold(); // The first: NextRange becomes Auto (a later 22 changes nothing).
    return true;
}
bool MeterCommands::close(uint32_t nowMs) {
    selector_.cancel(); key_.cancel(); range_.cancel(); cancelGroup();
    stroke_.close(nowMs);
    return !stroke_.active();
}
void MeterCommands::abandon() {
    selector_.cancel(); key_.cancel(); range_.cancel(); cancelGroup(); stroke_.abandon();
    fenced_ = false; // The UART and its session end: the next packets are another session's.
}
void MeterCommands::cancelOwner() {
    if (operation_ == Operation::Key) key_.cancel();
    else if (operation_ == Operation::Range) range_.cancel();
}
// NextRange: a short stroke (20, 21); AutoRange: a hold (20, 22). Other commands are not strokes.
bool MeterCommands::startStroke(CommandGroup command, uint32_t nowMs) {
    using End = RangeStroke::End;
    if (command != CommandGroup::NextRange && command != CommandGroup::AutoRange) return false;
    ended_ = false;
    return stroke_.start(command == CommandGroup::AutoRange ? End::Hold : End::Release, nowMs);
}
void MeterCommands::service(CommandPort &port, const Result &latest, uint32_t nowMs) {
    if (stroke_.active()) { serviceStroke(port, latest, nowMs); return; } // Nothing else until its end left.
    if (group_ == DataStatus::Busy) { serviceGroup(port, latest, nowMs); return; } // Nothing else is accepted meanwhile.
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
    if (event == Event::EndSent || event == Event::Released) { // Its effect counts from here.
        if (key) key_.sent(latest.sequence, nowMs);
        else range_.sent(latest.sequence, nowMs);
    }
    if (event == Event::Failed) {
        if (key) key_.fail();
        else range_.fail();
    }
    // A key held past its end (after a 22): its command is judged meanwhile (poll gives no command
    // then, only Ok or its deadline), so a long hold neither delays nor times out the confirmation.
    if (event == Event::EndSent || event == Event::Released) ended_ = true;
    CommandGroup none;
    if (key && ended_ && key_.busy()) key_.poll(latest, nowMs, none);
}
} // namespace openosc::dmm
