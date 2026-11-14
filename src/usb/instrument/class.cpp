#include "openosc/usb/instrument_class.hpp"
#include "openosc/usb/usb_device.hpp"
#include "openosc/usb/tmc_recovery.hpp"
#include "openosc/platform/critical_section.hpp"
#include "openosc/platform/dma_memory.hpp"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
#include <algorithm>
#include <cstring>

namespace openosc::usb {
InstrumentClass instrument OPENOSC_DMA_DATA;
namespace {
uint8_t configuration[] OPENOSC_DMA_DATA = {
    9,2,98,0,3,1,4,0xc0,50,
    9,4,0,0,2,0xfe,3,0,5,
    7,5,0x01,2,64,0,0, 7,5,0x81,2,64,0,0,
    8,11,1,2,2,2,1,5,
    9,4,1,0,1,2,2,1,5,
    5,0x24,0,0x10,0x01, 5,0x24,1,0,2, 4,0x24,2,2, 5,0x24,6,1,2,
    7,5,0x82,3,8,0,16,
    9,4,2,0,2,0x0a,0,0,5,
    7,5,0x03,2,64,0,0, 7,5,0x83,2,64,0,0
};
static_assert(sizeof configuration == 98);
uint8_t qualifier[] OPENOSC_DMA_DATA = {10,6,0,2,0xef,2,1,64,1,0};
uint8_t *config(uint16_t *size) { *size = sizeof configuration; return configuration; }
uint8_t *qualify(uint16_t *size) { *size = sizeof qualifier; return qualifier; }
void resetSerialSession() {
    instrument.clearMask|=2;
    auto &pipe=instrument.pipes[1];
    // DTR changes only the byte-stream session. Flushing OUT3 flushes the
    // shared RX FIFO during this very EP0 SETUP transaction and disturbs TMC.
    // Keep an active receive and any IN DMA buffer until its real completion.
    if (pipe.receiveReady) receive(1);
}
uint8_t init(USBD_HandleTypeDef *dev, uint8_t) {
    for (uint8_t ep : {uint8_t(1),uint8_t(3)}) {
        if (USBD_LL_OpenEP(dev, ep, USBD_EP_TYPE_BULK,64) != USBD_OK || USBD_LL_OpenEP(dev,ep | 0x80,USBD_EP_TYPE_BULK,64) != USBD_OK) return USBD_FAIL;
        dev->ep_out[ep].is_used = dev->ep_in[ep].is_used = 1;
    }
    if (USBD_LL_OpenEP(dev,0x82,USBD_EP_TYPE_INTR,8) != USBD_OK) return USBD_FAIL;
    dev->ep_in[2].is_used = 1;
    instrument.configured = true; ++instrument.generation; instrument.dtr = false;
    instrument.clearMask|=3;
    tmcRecovery.reset(instrument.generation); // SET_CONFIGURATION (USBTMC Table 17): a new session.
    for (unsigned i=0;i<2;++i) {
        instrument.pipes[i].transmitBusy=false;
        receive(i);
    }
    return USBD_OK;
}
uint8_t deinit(USBD_HandleTypeDef *dev, uint8_t) {
    instrument.configured = false; ++instrument.generation;
    tmcRecovery.reset(instrument.generation); // Bus reset or a new configuration (4.2.1.1, rule 7).
    for (uint8_t ep : {uint8_t(1),uint8_t(3)}) {
        USBD_LL_CloseEP(dev,ep); USBD_LL_CloseEP(dev,ep | 0x80);
        dev->ep_out[ep].is_used = dev->ep_in[ep].is_used = 0;
    }
    USBD_LL_CloseEP(dev,0x82); dev->ep_in[2].is_used = 0;
    return USBD_OK;
}
uint8_t setup(USBD_HandleTypeDef *dev, USBD_SetupReqTypedef *r) {
    // The core has already cleared HALT and armed the status stage. Resetting the
    // transport here would flush the RX FIFO shared with EP0 and the other interface.
    if (r->bmRequest == 0x02 && r->bRequest == USB_REQ_CLEAR_FEATURE && r->wValue == USB_FEATURE_EP_HALT && !r->wLength)
        return USBD_OK;
    auto *data = instrument.control; std::memset(data,0,24);
    const auto fail = [&]() { USBD_CtlError(dev,r); return uint8_t(USBD_FAIL); };
    const auto send = [&](unsigned count) { USBD_CtlSendData(dev,data,std::min<unsigned>(count,r->wLength)); return uint8_t(USBD_OK); };
    if ((r->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
        if (r->bRequest == USB_REQ_GET_INTERFACE && r->wIndex < 3 && !r->wValue && r->wLength == 1) return send(1);
        if (r->bRequest == USB_REQ_SET_INTERFACE && r->wIndex < 3 && !r->wValue && !r->wLength) {
            if (r->wIndex == 0) tmcRecovery.cancel(); // USBTMC Table 17.
            return USBD_OK;
        }
        if (r->bRequest == USB_REQ_GET_STATUS) return send(2);
        return fail();
    }
    if ((r->bmRequest & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_CLASS) return fail();
    if (r->wIndex == 1 && (r->bmRequest & 31) == USB_REQ_RECIPIENT_INTERFACE) {
        if (r->bRequest == 0x21 && r->bmRequest == 0xa1 && r->wLength == 7) {
            USBD_CtlSendData(dev,instrument.lineCoding,7); return USBD_OK;
        }
        if (r->bRequest == 0x20 && r->bmRequest == 0x21 && r->wLength == 7) { USBD_CtlPrepareRx(dev,instrument.lineCoding,7); return USBD_OK; }
        if (r->bRequest == 0x22 && r->bmRequest == 0x21 && !r->wLength) {
            const bool dtr = r->wValue & 1; if (dtr != instrument.dtr) resetSerialSession();
            instrument.dtr = dtr; return USBD_OK;
        }
        if (r->bRequest == 0x23 && r->bmRequest == 0x21 && !r->wLength) return USBD_OK;
        return fail();
    }
    // USBTMC (interface 0, Bulk-OUT 0x01, Bulk-IN 0x81). Split transactions: TmcRecovery.
    using Kind = TmcRecovery::Kind;
    const auto put32 = [&](unsigned at, uint32_t value) { for (unsigned i = 0; i < 4; ++i) data[at+i] = uint8_t(value >> (8*i)); };
    const auto haltOut = [&]() { USBD_LL_StallEP(dev, 0x01); }; // Before the response (Tables 20, 32).
    if (r->bmRequest == 0xa1 && r->wIndex == 0 && !r->wValue) {
        if (r->bRequest == 7) { // Ready at any time; during a working split a no-operation (4.2.1.1 rule 3a).
            data[0] = tmcRecovery.admit() ? TmcRecovery::Success : TmcRecovery::SplitInProgress;
            data[2] = 0; data[3] = 1; return send(24);
        }
        if (r->bRequest == 5) {
            uint8_t tag;
            data[0] = tmcRecovery.initiate(Kind::Clear, 0, false, tag);
            if (data[0] == TmcRecovery::Success) haltOut();
            return send(1);
        }
        if (r->bRequest == 6) {
            uint32_t count;
            data[0] = tmcRecovery.check(Kind::Clear, data[1], count);
            return send(2);
        }
    }
    if (r->bmRequest == 0xa2 && (((r->bRequest == 1 || r->bRequest == 2) && r->wIndex == 0x01) || ((r->bRequest == 3 || r->bRequest == 4) && r->wIndex == 0x81))) {
        const bool initiate = r->bRequest & 1, out = r->wIndex == 0x01;
        if (initiate && !(r->wValue >> 8)) {
            const auto &pipe = instrument.pipes[0];
            const bool outQueued = pipe.receiveReady && pipe.consumed < pipe.received; // Bulk-OUT FIFO not empty.
            data[0] = tmcRecovery.initiate(out ? Kind::AbortOut : Kind::AbortIn, uint8_t(r->wValue), outQueued, data[1]);
            if (out && data[0] == TmcRecovery::Success) haltOut();
            return send(2);
        }
        if (!initiate && !r->wValue) {
            uint32_t count;
            uint8_t queued;
            data[0] = tmcRecovery.check(out ? Kind::AbortOut : Kind::AbortIn, queued, count);
            if (!out) data[1] = queued;
            put32(4, count);
            return send(8);
        }
    }
    return fail();
}
uint8_t dataIn(USBD_HandleTypeDef *, uint8_t ep) {
    if (ep == 1 || ep == 3) instrument.pipes[ep == 1 ? 0 : 1].transmitBusy = false;
    return USBD_OK;
}
uint8_t dataOut(USBD_HandleTypeDef *dev, uint8_t ep) {
    if (ep != 1 && ep != 3) return USBD_FAIL;
    auto &pipe = instrument.pipes[ep == 1 ? 0 : 1];
    pipe.received = USBD_LL_GetRxDataSize(dev,ep); pipe.consumed = 0; pipe.receiveReady = true;
    return USBD_OK;
}
}
void receive(unsigned index) {
    CriticalSection lock; auto &pipe = instrument.pipes[index];
    if (!instrument.configured) return;
    pipe.receiveReady = false; pipe.received = pipe.consumed = 0;
    USBD_LL_PrepareReceive(&usbDevice,index ? 3 : 1,pipe.receive,64);
}
bool current(unsigned index, uint32_t generation) {
    return instrument.configured && instrument.generation == generation && !(instrument.clearMask & (1U << index));
}
void haltTmcOut(uint32_t generation) {
    CriticalSection lock;
    if (current(0, generation)) USBD_LL_StallEP(&usbDevice, 0x01);
}
bool transmit(unsigned index, size_t size, uint32_t generation) {
    CriticalSection lock; auto &pipe = instrument.pipes[index];
    if (!instrument.configured || instrument.generation != generation || (instrument.clearMask & (1U << index)) || pipe.transmitBusy || size > TransferBytes) return false;
    pipe.transmitBusy = true;
    if (USBD_LL_Transmit(&usbDevice,index ? 0x83 : 0x81,pipe.transmit,size) != USBD_OK) { pipe.transmitBusy = false; return false; }
    return true;
}
USBD_ClassTypeDef instrumentClass = {init,deinit,setup,nullptr,nullptr,dataIn,dataOut,nullptr,nullptr,nullptr,config,config,config,qualify};
} // namespace openosc::usb
