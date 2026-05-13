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

#ifndef NRF54L15DK_H
#define NRF54L15DK_H

#include "nrf_gpio.h"

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
#define LEDS_NUMBER         4
#define LED_PRIMARY_PIN     0  // LED0 on nRF54L15-DK
#define LED_SECONDARY_PIN   1  // LED1 on nRF54L15-DK
#define LED_STATE_ON        0

/*------------------------------------------------------------------*/
/* BUTTON
 *------------------------------------------------------------------*/
#define BUTTONS_NUMBER      4
#define BUTTON_1            0  // Button 1 on nRF54L15-DK
#define BUTTON_2            1  // Button 2 on nRF54L15-DK
#define BUTTON_PULL         NRF_GPIO_PIN_PULLUP

/*------------------------------------------------------------------*/
/* UART (VCOM via J-Link CDC)
 *------------------------------------------------------------------*/
#define RX_PIN_NUMBER        NRF_GPIO_PIN_MAP(1, 5)
#define TX_PIN_NUMBER        NRF_GPIO_PIN_MAP(1, 4)
#define CTS_PIN_NUMBER       NRF_GPIO_PIN_MAP(1, 7)
#define RTS_PIN_NUMBER       NRF_GPIO_PIN_MAP(1, 6)

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER    "Nordic"
#define BLEDIS_MODEL           "nRF54L15-DK"

#endif // NRF54L15DK_H
