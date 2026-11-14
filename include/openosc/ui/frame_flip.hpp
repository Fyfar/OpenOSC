#pragma once
#include <cstdint>

namespace openosc {
// The LTDC's double-buffer flip without waiting, and who owns which buffer. present() points the
// layer at the new frame and asks for a reload in the next vertical blanking; done() hands the
// old front back to the renderer only when the panel provably no longer reads it:
// - the hardware cleared the reload request (the new frame is being scanned);
// - the controller has been off for longer than a frame (QuietMs): it reads no buffer;
// - the reload never came within TimeoutMs (a stalled pixel clock): the controller is reset,
//   which stops its reads, and set up again on the new frame (restart()).
// Switched on again (started()), the controller is reset and set up on the latest presented
// frame before it scans: a reload request left over from the off time (VBR without blanking, IMR
// while off) never decides which buffer it reads.
// Otherwise both buffers stay with the panel and the renderer waits: never released on a count.
class FrameFlip {
public:
    class Port {
    public:
        virtual void show(const void *frame, bool immediate) = 0; // Layer address, reload.
        virtual bool reloading() const = 0;                         // The reload request is pending.
        virtual bool scanning() const = 0;                          // The controller is on.
        virtual bool restart(const void *frame) = 0;                // Reset, set up on `frame`, on.
    protected:
        ~Port() = default;
    };
    static constexpr uint32_t TimeoutMs = 50, QuietMs = 20; // A frame takes under 7 ms.
    FrameFlip() = default; // attach() before use.
    // `front`: the buffer the controller scans at first.
    explicit FrameFlip(Port &port, const void *front) : port_(&port), front_(front) {}
    void attach(Port &port, const void *front) { port_ = &port; front_ = front; }
    void present(const void *frame, uint32_t now);
    bool pending() const { return frame_ != nullptr; }
    // The old front is free (the new frame is on screen): returns the new front once, else null.
    const void *done(uint32_t now);
    void stopped(uint32_t now) { stoppedAt_ = now; stopped_ = true; } // The controller was switched off.
    bool stopped() const { return stopped_; }
    // Switch on: restart() on the pending frame, else the front. True when it scans; then a
    // pending frame is the new front, returned once in `shown` (else null). False: nothing changes
    // hands and the flip stays stopped (the caller keeps the controller off).
    bool started(const void *&shown);
    const void *front() const { return front_; }
    uint32_t restarts() const { return restarts_; }

private:
    const void *release();
    Port *port_ = nullptr;
    const void *frame_ = nullptr, *front_ = nullptr;
    uint32_t at_ = 0, stoppedAt_ = 0, restarts_ = 0;
    bool stopped_ = false;
};
} // namespace openosc
