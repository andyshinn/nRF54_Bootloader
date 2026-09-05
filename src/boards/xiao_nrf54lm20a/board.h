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

/* Pin assignments taken from the Zephyr board definition for the
 * Seeed Studio XIAO nRF54LM20A:
 *   boards/seeed/xiao_nrf54lm20a/xiao_nrf54lm20a_nrf54lm20a-common.dtsi
 *   boards/seeed/xiao_nrf54lm20a/xiao_nrf54lm20a_nrf54lm20a-pinctrl.dtsi
 *   boards/seeed/xiao_nrf54lm20a/seeed_xiao_connector.dtsi
 */

#ifndef _XIAO_NRF54LM20A
#define _XIAO_NRF54LM20A

#include "nrf_gpio.h"

#define _PINNUM(port, pin)    ((port)*32 + (pin))

/*------------------------------------------------------------------*/
/* LED
 *
 * On-board RGB LED driven by PWM20 (all channels active low):
 *   red   P1.22, blue P1.23, green P1.24
 * Zephyr aliases led0 to the blue LED; we use the red one as the primary
 * status LED and blue as the secondary, matching the usual bootloader
 * red = activity / blue = connected convention.
 *------------------------------------------------------------------*/
#define LEDS_NUMBER           2
#define LED_PRIMARY_PIN       _PINNUM(1, 22)  // Red
#define LED_SECONDARY_PIN     _PINNUM(1, 23)  // Blue
#define LED_STATE_ON          0

#define NEOPIXELS_NUMBER      0

/*------------------------------------------------------------------*/
/* BUTTON
 *
 * The board has a single user button (Zephyr sw0) on P0.09, active low
 * with an internal pull-up. The reset button is handled by hardware.
 *------------------------------------------------------------------*/
#define BUTTONS_NUMBER        1
#define BUTTON_1              _PINNUM(0, 9)   // User / BOOT button (sw0)
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

/*------------------------------------------------------------------*/
/* UART — serial DFU over the USB connector
 *
 * The USB port goes to the on-board SAMD11 CMSIS-DAP probe (VID 0x2886,
 * PID 0x0068), whose CDC bridge lands on P1.11 (nRF TX) / P1.10 (nRF RX).
 * These are the pins nrfutil reaches when the host talks to the USB port;
 * the Arduino core calls the same pair Serial2 and describes it as the
 * board's debug console (PIN_SERIAL2_TX/RX in
 * variants/xiao_nrf54lm20a/variant.h).
 *
 * Do NOT use D6/D7 (P1.08/P1.09) here — that is the core's Serial1 on the
 * XIAO header, which the USB port cannot reach.
 *
 * P1 is unreachable from SERIAL00, so board.cmake / board.mk select UARTE20
 * via UART_INSTANCE.
 *------------------------------------------------------------------*/
#define TX_PIN_NUMBER         _PINNUM(1, 11)
#define RX_PIN_NUMBER         _PINNUM(1, 10)
#define CTS_PIN_NUMBER        0xFFFFFFFF
#define RTS_PIN_NUMBER        0xFFFFFFFF

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "XIAO nRF54LM20A"

#endif // _XIAO_NRF54LM20A
