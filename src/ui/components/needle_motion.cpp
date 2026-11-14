#include "openosc/ui/components/needle_motion.hpp"
#include <algorithm>
#include <cmath>

namespace openosc::ui {
namespace {
constexpr float Omega = 2 * 3.14159265f * NeedleMotion::Frequency;
constexpr float RestPosition = 4e-4f, RestVelocity = 8e-3f; // Below a pixel of the dial's sweep.
constexpr float Rebound = .25f; // A peg returns a quarter of the speed.
} // namespace
void NeedleMotion::reset(float position) {
    position_ = target_ = std::isfinite(position) ? std::clamp(position, Low, High) : 0;
    velocity_ = 0;
    clocked_ = false;
    settled_ = true;
}
void NeedleMotion::target(float value) {
    if (!std::isfinite(value)) return;
    value = std::clamp(value, Low, High);
    if (value == target_) return;
    target_ = value;
    settled_ = false;
}
bool NeedleMotion::step(uint32_t now) {
    if (!clocked_) { clocked_ = true; last_ = now; return false; }
    uint32_t elapsed = now - last_;
    last_ = now;
    if (settled_ || !elapsed) return false;
    if (elapsed > MaxGapMs) elapsed = ResumeMs;
    const float before = position_;
    for (uint32_t done = 0; done < elapsed; done += StepMs) {
        const float h = float(std::min(StepMs, elapsed - done)) * 1e-3f;
        // Semi-implicit Euler: stable for these stiffness and step sizes.
        velocity_ += (Omega * Omega * (target_ - position_) - 2 * Damping * Omega * velocity_) * h;
        position_ += velocity_ * h;
        if (position_ < Low) { position_ = Low; velocity_ = -velocity_ * Rebound; }
        if (position_ > High) { position_ = High; velocity_ = -velocity_ * Rebound; }
    }
    if (!std::isfinite(position_) || !std::isfinite(velocity_)) { reset(target_); return true; }
    if (std::abs(position_ - target_) < RestPosition && std::abs(velocity_) < RestVelocity) {
        position_ = target_; velocity_ = 0; settled_ = true;
    }
    return position_ != before;
}
} // namespace openosc::ui
