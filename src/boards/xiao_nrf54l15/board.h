#ifndef _XIAO_NRF54L15
#define _XIAO_NRF54L15

#include "nrf_gpio.h"

// User LED P2.00 (active low)
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       NRF_GPIO_PIN_MAP(2, 0)
#define LED_STATE_ON          0

#define NEOPIXELS_NUMBER      0

// User button P0.00: held at reset -> serial DFU
#define BUTTONS_NUMBER        1
#define BUTTON_1              NRF_GPIO_PIN_MAP(0, 0)
#define BUTTON_DFU            BUTTON_1
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

// SAMD11 USB bridge on UARTE20 (P1 pins)
#define BOARD_UARTE_INSTANCE   NRF_UARTE20
#define BOARD_UARTE_IRQHandler SERIAL20_IRQHandler
#define TX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 9)
#define RX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 8)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "XIAO nRF54L15"

#endif // _XIAO_NRF54L15
