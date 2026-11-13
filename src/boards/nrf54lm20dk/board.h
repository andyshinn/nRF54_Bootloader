#ifndef _NRF54LM20DK_H
#define _NRF54LM20DK_H

#include "nrf_gpio.h"

// LED0 P1.22, LED1 P1.25 (active high, unlike the nRF54L15-DK)
#define LEDS_NUMBER           2
#define LED_PRIMARY_PIN       NRF_GPIO_PIN_MAP(1, 22)
#define LED_SECONDARY_PIN     NRF_GPIO_PIN_MAP(1, 25)
#define LED_STATE_ON          1

#define NEOPIXELS_NUMBER      0

// BTN0 P1.26 (DFU), BTN1 P1.09 (DFU + BTN1 -> OTA)
#define BUTTONS_NUMBER        2
#define BUTTON_1              NRF_GPIO_PIN_MAP(1, 26)
#define BUTTON_2              NRF_GPIO_PIN_MAP(1, 9)
#define BUTTON_DFU            BUTTON_1
#define BUTTON_DFU_OTA        BUTTON_2
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

// VCOM0 of the on-board J-Link on UARTE20 (P1 pins)
#define BOARD_UARTE_INSTANCE   NRF_UARTE20
#define BOARD_UARTE_IRQHandler SERIAL20_IRQHandler
#define TX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 16)
#define RX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 17)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// USB (J3: serial DFU over CDC, UF2 drive)
//--------------------------------------------------------------------+
// Nothing is registered for this board. The application keeps TinyUSB's default 0x239A:0xCAFE;
// the bootloader takes the same PID with bit 15 clear, as Adafruit's bootloaders do, so the
// host gives it a port of its own: 0x4AFE with CDC only (the application asked for serial
// DFU), 0x4AFD with CDC + the UF2 drive (button, or no valid application).
#define USB_DESC_VID           0x239A
#define USB_DESC_UF2_PID       0x4AFD
#define USB_DESC_CDC_ONLY_PID  0x4AFE

#define UF2_PRODUCT_NAME       "Nordic nRF54LM20-DK"
#define UF2_VOLUME_LABEL       "LM20DK-BOOT"
#define UF2_BOARD_ID           "nRF54LM20-DK"
#define UF2_INDEX_URL          "https://www.nordicsemi.com/Products/Development-hardware/nRF54LM20-DK"

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Nordic"
#define BLEDIS_MODEL          "NRF54LM20-DK"

#endif
