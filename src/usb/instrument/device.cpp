#include "openosc/usb/instrument_usb.hpp"
#include "openosc/usb/instrument_class.hpp"
#include "openosc/usb/usb_device.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/critical_section.hpp"
#include "openosc/platform/dma_memory.hpp"
#include "openosc/platform/cooperative_service.hpp"
#ifdef OPENOSC_USB_TRACE
#include "openosc/platform/usb_trace.hpp"
#endif

namespace openosc {
USBD_HandleTypeDef usbDevice OPENOSC_DMA_DATA {};
PCD_HandleTypeDef usbPcd OPENOSC_DMA_DATA {};
volatile bool usbHostClaimed = false;
volatile uint32_t usbLastActivity = 0;
volatile uint32_t usbResets = 0;
bool InstrumentUsb::start() {
    if (!allowed_) return false;
    if (active_) return true;
    if (USBD_Init(&usbDevice,&usb::instrumentDescriptors,0) != USBD_OK ||
        USBD_RegisterClass(&usbDevice,&usb::instrumentClass) != USBD_OK) {
        USBD_DeInit(&usbDevice); return false;
    }
    // Factory/Recovery may leave a connected core while validating the application. Give the
    // host a real detach interval before presenting a different USB function, unless stop()
    // already detached long enough (ReattachPlan).
    if (reattach_.needsDetach(Board::milliseconds())) {
        USBD_LL_Stop(&usbDevice);
        Board::delay(usb::ReattachPlan::DetachMs);
    }
    if (USBD_Start(&usbDevice) != USBD_OK) { USBD_DeInit(&usbDevice); return false; }
    active_=true; attachedAt_=Board::milliseconds(); reattach_.started();
    CooperativeService::install([](void *context,uint32_t now) {
        static_cast<InstrumentUsb*>(context)->background(now);
    },this);
    return true;
}
void InstrumentUsb::stop() {
    CooperativeService::remove(this);
    releaseOutNak(); // Not left set when the controller stops.
    reattach_.cancel(); // Also when already stopped: a pending planned attach does not outlive a stop.
    if (active_) {
        { CriticalSection lock; USBD_DeInit(&usbDevice); active_ = false; }
        reattach_.stopped(Board::milliseconds());
    }
    tmc_.reset(); serialEngine_.reset(); serialZlp_ = false;
}
bool InstrumentUsb::connected() const {
    return active_ && usb::instrument.configured && usbDevice.dev_state == USBD_STATE_CONFIGURED;
}
bool InstrumentUsb::hostActive(uint32_t now) const {
    return connected() || usbResets != busResets_ || uint32_t(now-busActivityAt_) < HostQuietMs;
}
void InstrumentUsb::poll(uint32_t now) {
    if (usbResets != busResets_) { busResets_ = usbResets; busActivityAt_ = now; }
    if (polling_) return;
    Polling guard(polling_);
    if (reattach_.due(now)) start();
    if (!active_) return;
    const auto *registers = reinterpret_cast<const USB_OTG_DeviceTypeDef*>(
        reinterpret_cast<uintptr_t>(usbPcd.Instance) + USB_OTG_DEVICE_BASE);
    // A suspended host still owns the configured device, so it must not be reattached.
    const bool enumerated=connected() || (active_ && usb::instrument.configured && usbDevice.dev_state==USBD_STATE_SUSPENDED &&
                                          usbDevice.dev_old_state==USBD_STATE_CONFIGURED);
    if (watchdog_.update(now,usbResets,boot_diagnostics.usbReads,enumerated && (registers->DCFG & USB_OTG_DCFG_DAD))) {
        // HAL_PCD_DevDisconnect drives D+ low; the host then enumerates afresh.
#ifdef OPENOSC_USB_TRACE
        usbtrace::mark(usbtrace::Tag::Reattach, usbResets);
#endif
        HAL_PCD_DevDisconnect(&usbPcd);
        Board::delay(10);
        HAL_PCD_DevConnect(&usbPcd); attachedAt_=now;
    }
    synchronize();
    if (!connected()) return;
    serialEngine_.poll(now);
    serviceTmc(now, true); // USBTMC: split transactions first, then the bulk pipe.
    transfer(now);
    boot_diagnostics.usbState = usbDevice.dev_state;
}
}
extern "C" void *recovery_usb_allocate(uint32_t) { return nullptr; }
extern "C" void recovery_usb_free(void *) {}
extern "C" void recovery_usb_delay(uint32_t ms) { openosc::Board::delay(ms); }
