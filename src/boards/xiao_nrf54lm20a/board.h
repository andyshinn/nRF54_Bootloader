#ifndef _XIAO_NRF54LM20A
#define _XIAO_NRF54LM20A

#include "nrf_gpio.h"

// RGB LED, red P1.22 and blue P1.23 (active low)
#define LEDS_NUMBER           2
#define LED_PRIMARY_PIN       NRF_GPIO_PIN_MAP(1, 22)
#define LED_SECONDARY_PIN     NRF_GPIO_PIN_MAP(1, 23)
#define LED_STATE_ON          0

#define NEOPIXELS_NUMBER      0

// User button P0.09: held at reset -> serial DFU
#define BUTTONS_NUMBER        1
#define BUTTON_1              NRF_GPIO_PIN_MAP(0, 9)
#define BUTTON_DFU            BUTTON_1
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

// SAMD11 USB bridge on UARTE20 (P1 pins)
#define BOARD_UARTE_INSTANCE   NRF_UARTE20
#define BOARD_UARTE_IRQHandler SERIAL20_IRQHandler
#define TX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 11)
#define RX_PIN_NUMBER         NRF_GPIO_PIN_MAP(1, 10)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define DEVICE_NAME           "XIAODFU"
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "XIAO nRF54LM20A"

#endif // _XIAO_NRF54LM20A
