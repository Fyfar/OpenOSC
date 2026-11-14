#include "openosc/usb/instrument_class.hpp"
#include "openosc/board/current.hpp"
#include "openosc/platform/dma_memory.hpp"
#include "usbd_ctlreq.h"

namespace {
// Development VID/PID, shared with Recovery. The product/class strings distinguish the modes.
uint8_t deviceDescriptor[] OPENOSC_DMA_DATA = {18,1,0,2,0xef,2,1,64,0x83,4,0x2a,0x57,0,2,1,2,3,1};
uint8_t language[] OPENOSC_DMA_DATA = {4,3,9,4};
uint8_t stringBuffer[128] OPENOSC_DMA_DATA;
uint8_t *device(USBD_SpeedTypeDef, uint16_t *size) { *size = sizeof deviceDescriptor; return deviceDescriptor; }
uint8_t *lang(USBD_SpeedTypeDef, uint16_t *size) { *size = sizeof language; return language; }
uint8_t *string(const char *value, uint16_t *size) { USBD_GetString(reinterpret_cast<uint8_t*>(const_cast<char*>(value)), stringBuffer, size); return stringBuffer; }
uint8_t *manufacturer(USBD_SpeedTypeDef, uint16_t *size) { return string("OpenOSC", size); }
uint8_t *product(USBD_SpeedTypeDef, uint16_t *size) { return string(openosc::board::Current::InstrumentProduct, size); }
uint8_t *config(USBD_SpeedTypeDef, uint16_t *size) { return string("USBTMC + SCPI serial", size); }
uint8_t *interface(USBD_SpeedTypeDef, uint16_t *size) { return string("SCPI", size); }
uint8_t *serial(USBD_SpeedTypeDef, uint16_t *size) {
    char value[25]; const auto *uid = reinterpret_cast<const uint32_t*>(UID_BASE);
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned word = 0; word < 3; ++word)
        for (unsigned i = 0; i < 8; ++i) value[word*8+i] = hex[(uid[word] >> (28-4*i)) & 15];
    value[24] = 0; return string(value, size);
}
}
namespace openosc::usb {
USBD_DescriptorsTypeDef instrumentDescriptors = {device,lang,manufacturer,product,serial,config,interface};
}
