#pragma once
#include <cstdint>

namespace openosc {
// The NOR SPI DMA (SpiDma, DMA2 streams 3/4) and the scope capture share DMA2's one memory port; a
// save during a fast capture overruns a capture lane's FIFO (FE, HIL 2026-11-15, with the buffers in
// AXI, in SRAM4 and word-packed alike). A transfer is therefore admitted, not just started:
// - begin(), before the transfer touches its buffers or enables a stream: the capture yields (its
//   client stops a fast record capture). False: it did not, and nothing of the transfer starts.
// - while admitted, active(): no capture starts (DataAccess::startCapture waits), and no second
//   transfer is admitted (the streams and buffers are the one in progress's): begin() refuses it.
// - end(stopped, retry), after both streams are proven stopped and the received data copied: the
//   barrier goes. A stop that was not proven keeps it (the buffers may still be written) and keeps
//   `retry`; begin() and service() try it again (RetryMs apart in service), and the barrier goes once
//   it succeeds, so neither the capture nor a clock change waits on it for good. Only that retry may
//   follow an ended transfer whose stop failed; a transfer still in progress admits nothing.
// Main-loop only (FatFs callers); no interrupt uses it. One bus: the state is static, as for
// CooperativeService. Without a client (Recovery, the boot before DataAccess) begin() admits at once.
class StorageAdmission {
public:
    class Capture {
    public:
        virtual bool yieldForStorage() = 0;
    protected:
        ~Capture() = default;
    };
    using Retry = bool (*)();
    static constexpr uint32_t RetryMs = 50;
    static void attach(Capture *capture) { capture_ = capture; }
    static void detach(Capture *capture) { if (capture_ == capture) capture_ = nullptr; }
    static bool begin() {
        if (stuck_) { if (!recover()) return false; } // The ended transfer's stop, proven first.
        else if (admitted_) return false;            // Nested in a transfer in progress: refused.
        if (capture_ && !capture_->yieldForStorage()) return false; // Refused: not admitted, nothing changed.
        admitted_ = true;
        return true;
    }
    static void end(bool stopped, Retry retry) {
        if (!admitted_ || stuck_) return;
        if (stopped) admitted_ = false;
        else stuck_ = retry; // The barrier stays until a stop is proven.
    }
    static bool active() { return admitted_; }
    static bool stuck() { return stuck_ != nullptr; }
    static void service(uint32_t now) {
        if (!stuck_ || int32_t(now - retryAt_) < 0) return;
        retryAt_ = now + RetryMs;
        recover();
    }

private:
    static bool recover() {
        if (!stuck_ || !stuck_()) return false;
        stuck_ = nullptr;
        admitted_ = false;
        return true;
    }
    inline static Capture *capture_ = nullptr;
    inline static Retry stuck_ = nullptr;
    inline static uint32_t retryAt_ = 0;
    inline static bool admitted_ = false; // A transfer in progress, or one ended whose stop is not proven.
};
} // namespace openosc
