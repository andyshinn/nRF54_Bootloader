/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "nrf.h"
#include "boards.h"
#include "tusb.h"

#include "usb_desc.h"

#if !defined(USB_DESC_VID) || !defined(USB_DESC_UF2_PID) || !defined(USB_DESC_CDC_ONLY_PID)
#error "USB DFU needs USB_DESC_VID, USB_DESC_UF2_PID and USB_DESC_CDC_ONLY_PID in board.h"
#endif

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_MSC,
  ITF_NUM_TOTAL
};

enum
{
  STRID_LANGUAGE = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_CDC,
  STRID_MSC
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_MSC_OUT     0x03
#define EPNUM_MSC_IN      0x83

#define CONFIG_CDC_LEN      (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define CONFIG_CDC_MSC_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)

// CDC + MSC (UF2 drive) unless the application asked for serial DFU only
static bool _cdc_only = false;

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
static tusb_desc_device_t desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,

  // Use Interface Association Descriptor (IAD) for CDC
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = USB_DESC_VID,
  .idProduct          = USB_DESC_UF2_PID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = STRID_MANUFACTURER,
  .iProduct           = STRID_PRODUCT,
  .iSerialNumber      = STRID_SERIAL,

  .bNumConfigurations = 1
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
// Bulk endpoints are 512 bytes at high speed and 64 at full speed
static uint8_t const desc_hs_cdc_msc[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_CDC_MSC_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
};

static uint8_t const desc_fs_cdc_msc[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_CDC_MSC_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

static uint8_t const desc_hs_cdc[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL - 1, 0, CONFIG_CDC_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),
};

static uint8_t const desc_fs_cdc[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL - 1, 0, CONFIG_CDC_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

TU_VERIFY_STATIC(sizeof(desc_hs_cdc_msc) == CONFIG_CDC_MSC_LEN, "size is not correct");
TU_VERIFY_STATIC(sizeof(desc_fs_cdc_msc) == CONFIG_CDC_MSC_LEN, "size is not correct");
TU_VERIFY_STATIC(sizeof(desc_hs_cdc) == CONFIG_CDC_LEN, "size is not correct");
TU_VERIFY_STATIC(sizeof(desc_fs_cdc) == CONFIG_CDC_LEN, "size is not correct");

static uint8_t const * configuration_for(bool high_speed)
{
  if (_cdc_only) return high_speed ? desc_hs_cdc : desc_fs_cdc;
  return high_speed ? desc_hs_cdc_msc : desc_fs_cdc_msc;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return configuration_for(tud_speed_get() == TUSB_SPEED_HIGH);
}

// A high-speed capable device describes how it would look at the other speed
static tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = 0x0200,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 1,
  .bReserved          = 0x00
};

uint8_t const * tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const *) &desc_device_qualifier;
}

static uint8_t desc_other_speed_config[CONFIG_CDC_MSC_LEN];

uint8_t const * tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index;

  // the other speed's configuration, typed as such
  uint8_t const * other = configuration_for(tud_speed_get() != TUSB_SPEED_HIGH);
  memcpy(desc_other_speed_config, other, _cdc_only ? CONFIG_CDC_LEN : CONFIG_CDC_MSC_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  return desc_other_speed_config;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
static char const * const string_desc_arr[] =
{
  [STRID_MANUFACTURER] = BLEDIS_MANUFACTURER,
  [STRID_PRODUCT]      = UF2_PRODUCT_NAME " DFU",
  [STRID_CDC]          = UF2_PRODUCT_NAME " CDC",
  [STRID_MSC]          = UF2_PRODUCT_NAME " UF2",
};

static uint16_t _desc_str[32 + 1];

// The serial number is the chip's DEVICEID in hex, as the application's TinyUSB port reports it
static uint8_t serial_to_utf16(uint16_t * dst)
{
  uint32_t const serial[2] = { NRF_FICR->INFO.DEVICEID[1], NRF_FICR->INFO.DEVICEID[0] };
  static char const nibble_to_hex[] = "0123456789ABCDEF";
  uint8_t count = 0;

  for (uint8_t w = 0; w < 2; w++)
  {
    for (int8_t shift = 28; shift >= 0; shift -= 4)
    {
      dst[count++] = nibble_to_hex[(serial[w] >> shift) & 0x0F];
    }
  }

  return count;
}

// Enumerate as CDC + MSC or CDC only; the two get different PIDs so the host keeps them apart
void usb_desc_init(bool cdc_only)
{
  _cdc_only = cdc_only;
  desc_device.idProduct = cdc_only ? USB_DESC_CDC_ONLY_PID : USB_DESC_UF2_PID;
}

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;
  uint8_t chr_count;

  if (index == STRID_LANGUAGE)
  {
    _desc_str[1] = 0x0409; // English
    chr_count = 1;
  }
  else if (index == STRID_SERIAL)
  {
    chr_count = serial_to_utf16(_desc_str + 1);
  }
  else
  {
    if (index >= TU_ARRAY_SIZE(string_desc_arr) || string_desc_arr[index] == NULL) return NULL;

    char const * str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);
    if (chr_count > 32) chr_count = 32;

    // ASCII to UTF-16
    for (uint8_t i = 0; i < chr_count; i++)
    {
      _desc_str[1 + i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}
