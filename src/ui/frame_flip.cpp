#include "openosc/ui/frame_flip.hpp"

namespace openosc {
void FrameFlip::present(const void *frame, uint32_t now) {
    port_->show(frame, !port_->scanning()); // Off: no blanking will come; the address is set for the next start.
    frame_ = frame;
    at_ = now;
}
const void *FrameFlip::release() {
    front_ = frame_;
    frame_ = nullptr;
    return front_;
}
bool FrameFlip::started(const void *&shown) {
    shown = nullptr;
    if (!port_->restart(frame_ ? frame_ : front_)) return false;
    stopped_ = false;
    if (frame_) shown = release();
    return true;
}
const void *FrameFlip::done(uint32_t now) {
    if (!frame_) return nullptr;
    if (!port_->scanning()) // Off for longer than a frame: it reads neither buffer.
        return stopped_ && uint32_t(now - stoppedAt_) >= QuietMs ? release() : nullptr;
    if (!port_->reloading()) return release(); // The new frame is being scanned.
    if (uint32_t(now - at_) < TimeoutMs) return nullptr;
    // No blanking came: reset the controller (its reads stop), then show the new frame.
    if (!port_->restart(frame_)) return nullptr;
    ++restarts_;
    return release();
}
} // namespace openosc
