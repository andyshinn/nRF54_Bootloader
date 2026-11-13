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

#include "nrfx.h"
#include "boards.h"
#include "tusb.h"
#include "uf2/uf2.h"
#include "usb.h"
#include "usb_desc.h"

// Serial DFU only runs with the SoftDevice off, so nothing else uses the interrupt levels
#define USBHS_IRQ_PRIORITY  7

static bool _usb_armed   = false;   // usb_init() called, waiting for VBUS or running
static bool _usb_started = false;   // PCLK24M and the stack are up

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBHS_IRQHandler(void)
{
  tud_int_handler(0);
}

// VREGUSB has no documented status register. Like Zephyr's dwc2 quirk for the nRF54L, take
// bit 2 at offset 0x400 as "VBUS present" for the state at start-up; VBUSDETECTED covers a
// cable plugged in later.
static bool vbus_present(void)
{
  uint32_t const status = *(volatile uint32_t const *) ((uintptr_t) NRF_VREGUSB + 0x400);
  return (status & (1UL << 2)) || NRF_VREGUSB->EVENTS_VBUSDETECTED;
}

// The DWC2 registers can only be accessed once VBUS is valid and PCLK24M runs. PCLK24M keeps
// the HFXO running; that is fine here because serial DFU never enables the SoftDevice, and
// usb_teardown() stops it again before the application's SoftDevice wants to tune the HFXO.
static void usb_start(void)
{
  NRF_VREGUSB->EVENTS_VBUSDETECTED = 0;

  NRF_CLOCK->EVENTS_XO24MSTARTED = 0;
  NRF_CLOCK->TASKS_XO24MSTART = 1;
  while (!NRF_CLOCK->EVENTS_XO24MSTARTED) {}

  NVIC_SetPriority(USBHS_IRQn, USBHS_IRQ_PRIORITY);

  tusb_rhport_init_t const rh_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_HIGH
  };
  tusb_init(0, &rh_init);

  _usb_started = true;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
void usb_init(bool cdc_only)
{
  // CDC only when the application asked for serial DFU, else CDC + the UF2 drive
  usb_desc_init(cdc_only);
  uf2_init();

  NRF_VREGUSB->EVENTS_VBUSDETECTED = 0;
  NRF_VREGUSB->TASKS_START = 1;
  _usb_armed = true;

  // Without a cable the DFU loop keeps running (and feeding the watchdog); the stack starts
  // from usb_task() once VBUS shows up.
  usb_task();
}

void usb_task(void)
{
  if (!_usb_armed) return;

  if (!_usb_started)
  {
    if (!vbus_present()) return;
    usb_start();
  }

  tud_task();
  tud_cdc_write_flush();
}

void usb_teardown(void)
{
  if (!_usb_armed) return;
  _usb_armed = false;

  if (_usb_started)
  {
    // Leave the bus so the host drops the bootloader's port before the application attaches
    tud_disconnect();
    NRFX_DELAY_MS(20);

    tusb_deinit(0);
    NVIC_DisableIRQ(USBHS_IRQn);
    NVIC_ClearPendingIRQ(USBHS_IRQn);
    NRF_USBHS->ENABLE = 0;

    // The application's SoftDevice starts the HFXO itself and waits for XOTUNED, which never
    // comes from a crystal PCLK24M kept running; let it stop first.
    NRF_CLOCK->TASKS_XO24MSTOP = 1;
    for (uint32_t i = 0; (NRF_CLOCK->XO.STAT & CLOCK_XO_STAT_STATE_Msk) && i < 100000; i++) {}

    _usb_started = false;
  }

  NRF_VREGUSB->TASKS_STOP = 1;
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void)
{
  led_state(STATE_USB_MOUNTED);
}

void tud_umount_cb(void)
{
  led_state(STATE_USB_UNMOUNTED);
}
