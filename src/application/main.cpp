#include "openosc/platform/watchdog.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/usb_trace.hpp"
#include "openosc/platform/clocks.hpp"
#include "openosc/application/controller.hpp"
#include "openosc/update/crc32.hpp"
#include "openosc/update/layout.hpp"
#include <new>

int main() {
    using namespace openosc;
    if (!Board::initialize()) Watchdog::fatal(boot_diagnostics.error);
    boot_diagnostics.factoryCrc = Crc32::compute(reinterpret_cast<const void*>(layout::FactoryBase), layout::FactoryBytes);
    NorFlash nor;
    BlockDevice disk(nor);
    Filesystem fs(disk);
    if (nor.initialize() && disk.initialize()) {
        fs.mountPrepared();
        boot_diagnostics.flashStatus=fs.result();
    }
    Watchdog::progress(); // Boot stages are the boot's progress (one pass, WatchdogPolicy::OperationMs).
    PreferencesFile preferences(fs);
    preferences.load();
    Watchdog::progress();
    if (!ClockControl::selectPerformance(preferences.values().scope.runMode)) {
        boot_diagnostics.error=4;
        // Keep a usable instrument if the profile cannot be locked; no high-rate metadata is fabricated.
        if (!ClockControl::selectPerformance(0)) Watchdog::fatal(4);
    }
#ifdef OPENOSC_USB_TRACE
    // Both the DWT timestamp scale and TIM7 divider depend on the selected profile.
    usbtrace::start();
#endif
    ThemeStore themes(fs);
    LvglDisplay display;
    ScreenshotWriter screenshots(fs);
    // The service lives for the complete firmware lifetime. It holds no DMA memory (the NOR, UART
    // and capture buffers are separate), so it lives in DTCM and AXI keeps room for the arena.
    alignas(DataAccess) static uint8_t dataStorage[sizeof(DataAccess)];
    DataAccess& data = *new (dataStorage) DataAccess(fs);
    ApplicationController controller(display, nor, fs, preferences, themes, screenshots,data);
    controller.run();
}
