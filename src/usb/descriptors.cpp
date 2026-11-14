#include "openosc/usb/usb_device.hpp"
#include "openosc/board/current.hpp"
#include "usbd_ctlreq.h"

namespace {
alignas(4) uint8_t descriptor[] = {
    18, USB_DESC_TYPE_DEVICE, 0x00, 0x02, 0, 0, 0, 64,
    0x83, 0x04, 0x2a, 0x57, 0x00, 0x01, 1, 2, 3, 1
};
alignas(4) uint8_t language[] = {4, USB_DESC_TYPE_STRING, 9, 4};
alignas(4) uint8_t stringBuffer[256];
uint8_t* device(USBD_SpeedTypeDef, uint16_t* size) { *size = sizeof descriptor; return descriptor; }
uint8_t* lang(USBD_SpeedTypeDef, uint16_t* size) { *size = sizeof language; return language; }
uint8_t* string(const char* value, uint16_t* size) {
    USBD_GetString(reinterpret_cast<uint8_t*>(const_cast<char*>(value)), stringBuffer, size);
    return stringBuffer;
}
uint8_t* manufacturer(USBD_SpeedTypeDef, uint16_t* size) { return string("OpenOSC", size); }
uint8_t* product(USBD_SpeedTypeDef, uint16_t* size) { return string(openosc::board::Current::RecoveryProduct, size); }
uint8_t* configuration(USBD_SpeedTypeDef, uint16_t* size) { return string("Recovery MSC", size); }
uint8_t* interfaceName(USBD_SpeedTypeDef, uint16_t* size) { return string("External Flash", size); }
uint8_t* serial(USBD_SpeedTypeDef, uint16_t* size) {
    char result[25];
    const auto* uid = reinterpret_cast<const uint32_t*>(UID_BASE);
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned word = 0; word < 3; ++word)
        for (unsigned i = 0; i < 8; ++i) result[word * 8 + i] = hex[(uid[word] >> (28 - 4 * i)) & 15];
    result[24] = 0;
    return string(result, size);
}
}
namespace openosc {
USBD_DescriptorsTypeDef recoveryDescriptors = {device, lang, manufacturer, product, serial, configuration, interfaceName};
}

