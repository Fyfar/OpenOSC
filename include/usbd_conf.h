#pragma once
#include <stdint.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#ifdef OPENOSC_APPLICATION
#define USBD_MAX_NUM_INTERFACES 3U
#else
#define USBD_MAX_NUM_INTERFACES 1U
#endif
#define USBD_MAX_NUM_CONFIGURATION 1U
#define USBD_MAX_STR_DESC_SIZ 256U
#define USBD_SELF_POWERED 1U
#define USBD_DEBUG_LEVEL 0U
#define USBD_LPM_ENABLED 0U
#define USBD_USER_REGISTER_CALLBACK 0U
#define MSC_MEDIA_PACKET 4096U
#ifdef __cplusplus
extern "C" {
#endif
void* recovery_usb_allocate(uint32_t size);
void recovery_usb_free(void* pointer);
void recovery_usb_delay(uint32_t ms);
int recovery_usb_sync(uint8_t lun);
#ifdef __cplusplus
}
#endif
#define USBD_malloc recovery_usb_allocate
#define USBD_free recovery_usb_free
#define USBD_memset memset
#define USBD_memcpy memcpy
#define USBD_Delay recovery_usb_delay
#define USBD_UsrLog(...) ((void)0)
#define USBD_ErrLog(...) ((void)0)
#define USBD_DbgLog(...) ((void)0)

