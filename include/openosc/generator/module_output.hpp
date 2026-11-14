#pragma once
#include "openosc/generator/module_link.hpp"
#include "openosc/platform/resources.hpp"

namespace openosc::generator {
// ZT706S: the external generator module on USART3, PC10 TX / PC11 RX (AF7, pull-up), 9600 8N1, IRQ39,
// as the factory main and GEN (706S main 0802C278, GEN 2406840C). A setting is one 512-byte frame
// (ModuleWave) the TXE interrupt sends in ~0.53 s; nothing waits for it in the main loop. No DMA
// (DMA1 Stream0 is the Scope capture's) and no DAC: it neither loads the bus nor lowers the
// capture's maximum rate. The kernel clock is PCLK1, the mux UART5 (DMM) shares.
// Run-mode changes only between frames: clockBusy() while one is on the wire (the change is refused
// as Busy, nothing stopped), then the UART stops and starts again on the new PCLK1; a frame that was
// waiting for its acknowledge goes again whole. The output is not switched off for it.
// enabled() is what was asked (ON/OFF); applied what the module acknowledged (unknown after reset, a
// timeout, a stuck UART or while a frame is on its way). status(): Busy until acknowledged, Ok,
// Timeout (no acknowledge after ModuleLink::Attempts), HardwareError (UART did not start or finish).
class ModuleOutput {
public:
    enum class Applied : uint8_t { Unknown, Off, On };
    explicit ModuleOutput(PeripheralResources &) {}
    ~ModuleOutput() { stop(); }
    bool configure(const Settings &settings);   // Refuses what the module cannot do (Capabilities).
    bool enable();
    // OFF whenever the module may not be off: ON asked, or not acknowledged as off (the boot, after
    // a timeout or a stuck UART) - so a failed OFF is tried again; not again while one is queued.
    void disable();
    void service();
    bool clockBusy() const { return !link_.suspendable(); }
    bool suspendForClock();
    bool retime(bool enable);
    bool enabled() const { return requested_; }
    bool busy() const { return link_.busy(); }
    bool loadsBus() const { return false; }
    Applied applied() const { return applied_; }
    bool offConfirmed() const { return !busy() && applied_ == Applied::Off; }
    DataStatus status() const { return portFailed_ ? DataStatus::HardwareError : link_.status(); }
    const Settings &settings() const { return settings_; }
    const OutputPlan &plan() const { return plan_; }
    Snapshot snapshot() const { return {settings_, plan_, status(), requested_}; }

private:
    bool start();
    void stop();
    bool submit(bool on);
    ModuleLink link_;
    Settings settings_;
    OutputPlan plan_;
    Applied applied_ = Applied::Unknown;
    uint32_t sentEpoch_ = 0;            // The interrupt's TC count when the frame awaiting its ACK ended.
    bool requested_ = false, running_ = false, portFailed_ = false, onWire_ = false;
};
} // namespace openosc::generator
