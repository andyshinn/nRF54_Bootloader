/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach
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

#ifndef _XIAO_NRF54L15
#define _XIAO_NRF54L15

#include "nrf_gpio.h"

#define _PINNUM(port, pin)    ((port)*32 + (pin))

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
// User LED on P2.0 (confirmed from Zephyr device tree)
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       _PINNUM(2, 0)
#define LED_STATE_ON          0

#define NEOPIXELS_NUMBER      0

/*------------------------------------------------------------------*/
/* BUTTON
 *------------------------------------------------------------------*/
// Based on XIAO form factor, using typical button pins
// Reset button is handled by hardware
// User button (SW0) on expansion or D1 pin
#define BUTTONS_NUMBER        2
#define BUTTON_1              _PINNUM(1, 4)  // D0 on XIAO connector
#define BUTTON_2              _PINNUM(1, 5)  // D1 on XIAO connector
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

/*------------------------------------------------------------------*/
/* UART — D6(TX) / D7(RX) on XIAO connector
 *------------------------------------------------------------------*/
#define TX_PIN_NUMBER         _PINNUM(1, 11)
#define RX_PIN_NUMBER         _PINNUM(1, 12)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "XIAO nRF54L15"

#endif // _XIAO_NRF54L15
