#include "openosc/acquisition/mailbox.hpp"
#include "openosc/platform/critical_section.hpp"

namespace openosc::scope {
FrameLease::~FrameLease() { release(); }
FrameLease::FrameLease(FrameLease &&other) noexcept { *this = static_cast<FrameLease &&>(other); }
FrameLease &FrameLease::operator=(FrameLease &&other) noexcept {
    if (this == &other)
        return *this;
    release();
    view_ = other.view_;
    owner_ = other.owner_;
    other.owner_ = nullptr;
    other.view_ = {};
    return *this;
}
void FrameLease::release() {
    if (owner_) {
        owner_->release();
        owner_ = nullptr;
        view_ = {};
    }
}
bool FrameLease::share(FrameLease &out) const {
    if (!owner_ || out.valid() || owner_->readers_ == UINT32_MAX) return false;
    ++owner_->readers_;
    out.view_ = view_;
    out.owner_ = owner_;
    return true;
}
bool FrameMailbox::begin() {
    CriticalSection guard;
    if (state() != State::Idle)
        return false;
    state_.store(State::Capturing, std::memory_order_release);
    return true;
}
bool FrameMailbox::publish(const FrameView &frame) {
    if (state() != State::Capturing || !frame.valid())
        return false;
    frame_ = frame;
    state_.store(State::Ready, std::memory_order_release);
    return true;
}
bool FrameMailbox::acquire(FrameLease &lease) {
    if (lease.valid())
        return false;
    const State current = state();
    if (current != State::Ready && current != State::Leased)
        return false;
    if (readers_ == UINT32_MAX)
        return false;
    ++readers_;
    state_.store(State::Leased, std::memory_order_release);
    lease.view_ = frame_;
    lease.owner_ = this;
    return true;
}
bool FrameMailbox::cancel() {
    CriticalSection guard;
    if (state() != State::Capturing)
        return false;
    state_.store(State::Idle, std::memory_order_release);
    return true;
}
void FrameMailbox::release() {
    if (readers_ && --readers_ == 0)
        state_.store(State::Idle, std::memory_order_release);
}
} // namespace openosc::scope
