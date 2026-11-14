#pragma once
#include <cstdint>

namespace openosc::ui {
// The moving-coil needle: a damped spring towards the target, integrated over the real time
// between calls (sub-steps of at most StepMs, so the motion does not depend on the frame rate).
// Positions are fractions of the scale; the needle stops at the pegs just outside 0 and 1.
// A gap longer than MaxGapMs (the screen was hidden or busy) advances only one frame: no jump,
// no burst of work. A non-finite target is ignored; once settled the needle rests exactly on the
// target and step() reports no motion, so nothing is redrawn.
class NeedleMotion {
public:
    static constexpr float Low = -.035f, High = 1.06f; // The pegs.
    static constexpr float Frequency = 2.4f, Damping = .6f; // Hz; ~10 % overshoot, ~0.45 s settling.
    static constexpr uint32_t StepMs = 2, MaxGapMs = 100, ResumeMs = 16;
    void reset(float position = 0);           // At rest at `position`, the clock restarts.
    void target(float value);                 // Clamped to the pegs; NaN/Inf leave the target as it was.
    bool step(uint32_t nowMs);                // True if the needle moved.
    void pause() { clocked_ = false; }        // The next step starts the clock again (no catch-up).
    float position() const { return position_; }
    float velocity() const { return velocity_; }
    float goal() const { return target_; }
    bool settled() const { return settled_; }

private:
    float position_ = 0, velocity_ = 0, target_ = 0;
    uint32_t last_ = 0;
    bool clocked_ = false, settled_ = true;
};
} // namespace openosc::ui
