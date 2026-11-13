#ifndef _LM20MESHNODE_H
#define _LM20MESHNODE_H

#include "nrf_gpio.h"

// LM20MeshNode rev 0.1 (https://github.com/cvaldess/LM20MeshNode): Minew ME54BS13 (nRF54LM20A)
// with an E22-900MM22S, an E108-GN04S and an nPM1300.

// Status LED P2.00 (green, active high). The nPM1300 drives the charge and error LEDs itself.
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       NRF_GPIO_PIN_MAP(2, 0)
#define LED_STATE_ON          1

#define NEOPIXELS_NUMBER      0

// Side buttons to GND, no external pull-ups: OK P1.28 (DFU), DOWN P1.25 (OK + DOWN -> OTA).
// UP is P1.27. P1.26 is the LoRa module's NRST and P1.09 the buzzer, so the DK's pins do not apply.
#define BUTTONS_NUMBER        2
#define BUTTON_1              NRF_GPIO_PIN_MAP(1, 28)
#define BUTTON_2              NRF_GPIO_PIN_MAP(1, 25)
#define BUTTON_DFU            BUTTON_1
#define BUTTON_DFU_OTA        BUTTON_2
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

// Debug UART test pads (UARTE00, P2 pins): TX P2.02 (TP201), RX P2.07 (TP202)
#define BOARD_UARTE_INSTANCE   NRF_UARTE00
#define BOARD_UARTE_IRQHandler SERIAL00_IRQHandler
#define TX_PIN_NUMBER         NRF_GPIO_PIN_MAP(2, 2)
#define RX_PIN_NUMBER         NRF_GPIO_PIN_MAP(2, 7)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// USB (USB-C J101: serial DFU over CDC, UF2 drive)
//--------------------------------------------------------------------+
// Same IDs as the nRF54LM20-DK: the application keeps TinyUSB's default 0x239A:0xCAFE and the
// bootloader takes the same PID with bit 15 clear, 0x4AFE with CDC only (the application asked
// for serial DFU) and 0x4AFD with CDC + the UF2 drive (button, or no valid application).
#define USB_DESC_VID           0x239A
#define USB_DESC_UF2_PID       0x4AFD
#define USB_DESC_CDC_ONLY_PID  0x4AFE

#define UF2_PRODUCT_NAME       "LM20MeshNode"
#define UF2_VOLUME_LABEL       "LM20MN-BOOT"
#define UF2_BOARD_ID           "LM20MeshNode-r0.1"
#define UF2_INDEX_URL          "https://github.com/cvaldess/LM20MeshNode"

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "CValdesS"
#define BLEDIS_MODEL          "LM20MeshNode"

#endif
