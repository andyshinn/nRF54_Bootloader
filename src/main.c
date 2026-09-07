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

/**
 * -# Receive start data packet.
 * -# Based on start packet, prepare NVM area to store received data.
 * -# Receive data packet.
 * -# Validate data packet.
 * -# Write Data packet to NVM.
 * -# If not finished - Wait for next packet.
 * -# Receive stop data packet.
 * -# Activate Image, boot application.
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#include "nrfx.h"
#include "nrf_clock.h"
#include "nrfx_power.h"
#include "nrfx_pwm.h"

#include "sdk_common.h"
#include "bootloader.h"
#include "bootloader_util.h"

#include "nrf.h"
#include "nrf_soc.h"
#include "nrf_nvic.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "nrf.h"
#include "ble_hci.h"
#include "app_scheduler.h"
#include "nrf_error.h"

#include "boards.h"

#include "pstorage_platform.h"
#include "nrf_mbr.h"
#include "pstorage.h"

/* nRF54L has no USB peripheral — serial DFU uses UART only */
#define usb_init(x)       led_state(STATE_USB_MOUNTED)
#define usb_teardown()

/*
 * Blinking patterns:
 * - DFU Serial     : LED Status blink
 * - DFU OTA        : LED Status & Conn blink at the same time
 * - DFU Flashing   : LED Status blink 2x fast
 * - Factory Reset  : LED Status blink 2x fast
 * - Fatal Error    : LED Status & Conn blink one after another
 */

/* Magic that written to NRF_POWER->GPREGRET by application when it wish to go into DFU
 * - DFU_MAGIC_OTA_APPJUM        : used by BLEDfu service, SD is already inited
 * - DFU_MAGIC_OTA_RESET         : entered by soft reset, SD is not inited yet
 * - DFU_MAGIC_SERIAL_ONLY_RESET : serial DFU only (UART)
 * - DFU_MAGIC_SKIP              : skip DFU entirely including double reset delay,
 *                                 Can be used with systemoff or quick reset to app
 *
 * Note: for DFU_MAGIC_OTA_APPJUM Softdevice must not initialized.
 * since it is already in application. In all other case of OTA SD must be initialized
 */
#define DFU_MAGIC_OTA_APPJUM            BOOTLOADER_DFU_START  // 0xB1
#define DFU_MAGIC_OTA_RESET             0xA8
#define DFU_MAGIC_SERIAL_ONLY_RESET     0x4e
#define DFU_MAGIC_SKIP                  0x6d

#define DFU_DBL_RESET_MAGIC             0x5A1AD5      // SALADS
#define DFU_DBL_RESET_APP               0x4ee5677e
#define DFU_DBL_RESET_DELAY             500

#define BOOTLOADER_VERSION_REGISTER     NRF_TIMER22->CC[0]
#define DFU_SERIAL_STARTUP_INTERVAL     1000

// Allow for using reset button essentially to swap between application and bootloader.
// This is controlled by a flag in the app and is the behavior of CPX and all Arcade boards when using MakeCode.
// DFU_DBL_RESET magic is used to determined which mode is entered
#define APP_ASKS_FOR_SINGLE_TAP_RESET() (*((uint32_t*)(DFU_BANK_0_REGION_START + 0x200)) == 0x87eeb07c)

#define BLEGAP_EVENT_LENGTH             12
#define BLEGATTS_HVN_QSIZE              12
#define BLEGATTC_WRCMD_QSIZE            2

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
/* Double-reset detection word. The address comes from ORIGIN(DBL_RESET) in the
 * board's linker script, which also keeps every output section off it; it used
 * to be a literal 0x200047F8 here that nothing tied to the region, so the two
 * could drift apart silently. */
extern uint32_t __dbl_reset_mem[];
uint32_t* dbl_reset_mem = __dbl_reset_mem;

// true if ble, false if serial
bool _ota_dfu = false;
bool _ota_connected = false;
bool _ota_was_connected = false;
bool _sd_inited = false;

/* Whether S145 is actually programmed where this build was told it lives.
 *
 * Worth asking before any sd_* call, because every one of them is an SVC that
 * src/sd_isr_nrf54l.S forwards to a handler address it reads out of a table at
 * SOFTDEVICE_BASE_ADDRESS. If the SoftDevice was never flashed, that table
 * reads as erased and the first SoftDevice call branches to 0xFFFFFFFF. The
 * SoftDevice info struct sits SOFTDEVICE_INFO_STRUCT_OFFSET into the image and
 * carries SD_MAGIC_NUMBER one word in. */
static inline bool softdevice_is_present(void) {
  uint32_t const info = SOFTDEVICE_BASE_ADDRESS + SOFTDEVICE_INFO_STRUCT_OFFSET;

  /* Bound the read first. The only vendored S145 v9.0.0 image is the nRF54L15
   * build, based at 0x00158C00, which is past the end of RRAM on the L10
   * (0x000FD000) and the L05 (0x0007D000) -- those boards have no SoftDevice
   * that fits, and reading an unmapped RRAM address bus-faults rather than
   * returning 0xFFFFFFFF. This folds to a constant per board. */
  if ((info + 8) > NRF_MEMORY_FLASH_SIZE) {
    return false;
  }

  return *(uint32_t const *)(info + 4) == SD_MAGIC_NUMBER;
}

bool is_ota(void) {
  return _ota_dfu;
}

static void check_dfu_mode(void);
static uint32_t ble_stack_init(void);

// Disable the SoftDevice if it is enabled.
static void disable_softdevice(void) {
  uint8_t sd_enabled = 0;
  sd_softdevice_is_enabled(&sd_enabled);
  if (sd_enabled == 1) {
    sd_softdevice_disable();
  }
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
int main(void) {
  // Populate Boot Address and MBR Param into MBR if not already
  // MBR_BOOTLOADER_ADDR/MBR_PARAM_PAGE_ADDR are used if available, else UICR registers are used
  // Note: skip it for now since this will prevent us to change the size of bootloader in the future
  // bootloader_mbr_addrs_populate();

  // Save bootloader version to pre-defined register, retrieved by application
  // TODO move to CF2
  BOOTLOADER_VERSION_REGISTER = (MK_BOOTLOADER_VERSION);

  board_init();
  bootloader_init();
  PRINTF("Bootloader Start\r\n");
  led_state(STATE_BOOTLOADER_STARTED);

  // When updating SoftDevice, bootloader will reset before swapping SD
  if (bootloader_dfu_sd_in_progress()) {
    led_state(STATE_WRITING_STARTED);

    bootloader_dfu_sd_update_continue();
    bootloader_dfu_sd_update_finalize();

    led_state(STATE_WRITING_FINISHED);
  }

  // Check all inputs and enter DFU if needed
  // Return when DFU process is complete (or not entered at all)
  check_dfu_mode();

  // Check if we must reenter the bootloader after reset, instead of 
  // launching the user application
  bool bootloader_must_be_reentered = bootloader_must_reset_to_self();

  // Reset peripherals
  board_teardown();

  /* Jump to application if valid
   * "Master Boot Record and SoftDevice initializaton procedure"
   * - SD_MBR_COMMAND_INIT_SD (if not already)
   * - sd_softdevice_disable()
   * - sd_softdevice_vector_table_base_set(APP_ADDR)
   * - jump to App reset
   */
  if (!bootloader_must_be_reentered && 
       bootloader_app_is_valid() && 
      !bootloader_dfu_sd_in_progress()) {
    PRINTF("App is valid\r\n");

    /* If the application jumped here with the SoftDevice already running
     * (buttonless DFU), shut it down before handing control back. This used to
     * sit behind is_sd_existed() together with an SD_MBR_COMMAND_INIT_SD call,
     * neither of which means anything on nRF54L: there is no MBR to command,
     * and the SoftDevice is never below the application. On a cold boot the
     * SoftDevice was never enabled and there is nothing to do -- and nothing to
     * ask, since sd_softdevice_is_enabled() is itself an SVC into it. */
    if (_sd_inited) {
      disable_softdevice();
    }

    // clear in case we kept DFU_DBL_RESET_APP there
    (*dbl_reset_mem) = 0;

    // start application
    PRINTF("Starting app...\r\n");
    bootloader_app_start();
  }

  // No application was loaded or we need to reenter the bootloader
  
  // Reset the system with the OTA DFU update in case we were in it, 
  // to allow completion of FLASHING, otherwise, default to normal reset
  if (_ota_was_connected) {
    NRF_POWER->GPREGRET[0] = DFU_MAGIC_OTA_RESET;
  }
  
  NVIC_SystemReset();
}

static void check_dfu_mode(void) {
  uint32_t const gpregret = NRF_POWER->GPREGRET[0];

  // SD is already Initialized in case of BOOTLOADER_DFU_OTA_MAGIC
  _sd_inited = (gpregret == DFU_MAGIC_OTA_APPJUM);

  // Start Bootloader in BLE OTA mode
  _ota_dfu = (gpregret == DFU_MAGIC_OTA_APPJUM) || (gpregret == DFU_MAGIC_OTA_RESET);

  /* ...but only if there is a SoftDevice to talk to. Serial DFU is the last way
   * back into a board that has no debugger attached, so falling back to it
   * beats faulting inside the first sd_* call. */
  if (_ota_dfu && !softdevice_is_present()) {
    PRINTF("SoftDevice not present, falling back to serial DFU\r\n");
    _ota_dfu    = false;
    _sd_inited  = false;
  }

  // Serial only mode
  bool const serial_only_dfu = (gpregret == DFU_MAGIC_SERIAL_ONLY_RESET);
  bool const dfu_skip        = (gpregret == DFU_MAGIC_SKIP);

  bool const reason_reset_pin = (NRF_RESET->RESETREAS & RESET_RESETREAS_RESETPIN_Msk) ? true : false;

  // start either serial or ble
  bool dfu_start = _ota_dfu || serial_only_dfu ||
                   (((*dbl_reset_mem) == DFU_DBL_RESET_MAGIC) && reason_reset_pin);

  // Clear GPREGRET if it is our values
  if (dfu_start || dfu_skip) {
    NRF_POWER->GPREGRET[0] = 0;
  }

  // skip dfu entirely
  if (dfu_skip) {
    return;
  }

  /*------------- Determine DFU mode (Serial, OTA, FRESET or normal) -------------*/
  // DFU button pressed
#if defined(BUTTON_DFU)
  dfu_start = dfu_start || button_pressed(BUTTON_DFU);
#endif

  // DFU + FRESET are pressed --> OTA
#if defined(BUTTON_DFU) && defined(BUTTON_DFU_OTA)
  _ota_dfu = _ota_dfu || (button_pressed(BUTTON_DFU) && button_pressed(BUTTON_DFU_OTA));
#endif

  bool const valid_app = bootloader_app_is_valid();
  bool const just_start_app = valid_app && !dfu_start && (*dbl_reset_mem) == DFU_DBL_RESET_APP;

  if (!just_start_app && APP_ASKS_FOR_SINGLE_TAP_RESET()) {
    dfu_start = 1;
  }

#ifdef DEFAULT_TO_OTA_DFU
  // Default to OTA DFU mode, instead of Serial DFU mode, if there is no app present, 
  // because otherwise, if there is no application, it will restart in Serial DFU mode,
  // making it IMPOSSIBLE to recover devices in the field if there are no user 
  // accessible USB ports
  if (!valid_app || dfu_start) {
    _ota_dfu = 1;
  }
#endif

  // App mode: Double Reset detection
  if (!(just_start_app || dfu_start || !valid_app)) {
    // Note: RESETREAS is not cleared by bootloader, it should be cleared by application upon init()
    if (reason_reset_pin) {
      // Register our first reset for double reset detection
      (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;

      // if RST is pressed during this delay (double reset)--> it will enter dfu
      NRFX_DELAY_MS(DFU_DBL_RESET_DELAY);
    }
  }

  if (APP_ASKS_FOR_SINGLE_TAP_RESET()) {
    (*dbl_reset_mem) = DFU_DBL_RESET_APP;
  } else {
    (*dbl_reset_mem) = 0;
  }

  // Enter DFU mode accordingly to input
  if (dfu_start || !valid_app) {
    if (_ota_dfu) {
      led_state(STATE_BLE_DISCONNECTED);
      /* No SD_MBR_COMMAND_INIT_SD here: nRF54L has no MBR, so that SVC had
       * nothing to reach and hung in the bootloader's own SVC_Handler.
       * sd_softdevice_enable() inside ble_stack_init() is what brings S145 up,
       * and it reaches the SoftDevice through the SVC forwarding installed by
       * src/sd_isr_nrf54l.S. */
      _sd_inited = true;
      ble_stack_init();
    } else {
      led_state(STATE_USB_UNMOUNTED);
      usb_init(serial_only_dfu);
    }

    // Initiate an update of the firmware.
    if (APP_ASKS_FOR_SINGLE_TAP_RESET() || serial_only_dfu) {
      // If serial DFU is not active in 3s, we restart into app.
      bootloader_dfu_start(_ota_dfu, 3000, true);
    } else {
      // No timeout if bootloader requires user action (double-reset).
      bootloader_dfu_start(_ota_dfu, 0, false);
    }

    if (_ota_dfu) {
      disable_softdevice();
    } else {
      usb_teardown();
    }
  }
}

//--------------------------------------------------------------------+
// BLE
//--------------------------------------------------------------------+

// Initializes the SoftDevice by following SD specs section
// "Master Boot Record and SoftDevice initializaton procedure"
static uint32_t ble_stack_init(void) {
  // Forward vector table to bootloader address so that we can handle BLE events
  sd_softdevice_vector_table_base_set(BOOTLOADER_REGION_START);

  // Enable Softdevice, Use Internal OSC to compatible with all boards
  nrf_clock_lf_cfg_t clock_cfg = {
      .source       = NRF_CLOCK_LF_SRC_RC,
      .rc_ctiv      = 16,
      .rc_temp_ctiv = 2,
      .accuracy     = NRF_CLOCK_LF_ACCURACY_250_PPM
  };
  #ifdef ANT_LICENSE_KEY
    sd_softdevice_enable(&clock_cfg, app_error_fault_handler, ANT_LICENSE_KEY);
  #else
    sd_softdevice_enable(&clock_cfg, app_error_fault_handler);
  #endif
  sd_nvic_EnableIRQ(SD_EVT_IRQn);

  /*------------- Configure BLE params  -------------*/
  extern uint32_t __data_start__[]; // defined in linker
  uint32_t ram_start = (uint32_t) __data_start__;

  ble_cfg_t blecfg;

  // Configure the maximum number of connections.
  varclr(&blecfg);
  blecfg.gap_cfg.role_count_cfg.adv_set_count = 1;
  blecfg.gap_cfg.role_count_cfg.periph_role_count = 1;
  blecfg.gap_cfg.role_count_cfg.central_role_count = 0;
  blecfg.gap_cfg.role_count_cfg.central_sec_count = 0;
  sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &blecfg, ram_start);

  // NRF_DFU_BLE_REQUIRES_BONDS
  varclr(&blecfg);
  blecfg.gatts_cfg.service_changed.service_changed = 1;
  sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &blecfg, ram_start);

  // ATT MTU
  varclr(&blecfg);
  blecfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_HIGH_BANDWIDTH;
  blecfg.conn_cfg.params.gatt_conn_cfg.att_mtu = BLEGATT_ATT_MTU_MAX;
  sd_ble_cfg_set(BLE_CONN_CFG_GATT, &blecfg, ram_start);

  // Event Length + HVN queue + WRITE CMD queue setting affecting bandwidth
  varclr(&blecfg);
  blecfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_HIGH_BANDWIDTH;
  blecfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
  blecfg.conn_cfg.params.gap_conn_cfg.event_length = BLEGAP_EVENT_LENGTH;
  sd_ble_cfg_set(BLE_CONN_CFG_GAP, &blecfg, ram_start);

  // HVN queue size
  varclr(&blecfg);
  blecfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_HIGH_BANDWIDTH;
  blecfg.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = BLEGATTS_HVN_QSIZE;
  sd_ble_cfg_set(BLE_CONN_CFG_GATTS, &blecfg, ram_start);

  // WRITE COMMAND queue size
  varclr(&blecfg);
  blecfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_HIGH_BANDWIDTH;
  blecfg.conn_cfg.params.gattc_conn_cfg.write_cmd_tx_queue_size = BLEGATTC_WRCMD_QSIZE;
  sd_ble_cfg_set(BLE_CONN_CFG_GATTC, &blecfg, ram_start);

  // Enable BLE stack.
  // Note: Interrupt state (enabled, forwarding) is not work properly if not enable ble
  sd_ble_enable(&ram_start);

#if BLEGATT_ATT_MTU_MAX > 23 || defined(GPIO_PA_PIN) || defined(GPIO_LNA_PIN)
  ble_opt_t  opt;
#endif

#if BLEGATT_ATT_MTU_MAX > 23
  varclr(&opt);
  opt.common_opt.conn_evt_ext.enable = 1; // enable Data Length Extension
  sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
#endif

#if defined(GPIO_PA_LNA_MODE_PIN)
  // Set PA / LNA Mode Pin: Low for Normal operation
  nrf_gpio_cfg_output(GPIO_PA_LNA_MODE_PIN);
  nrf_gpio_pin_clear(GPIO_PA_LNA_MODE_PIN);
#endif

#if defined(GPIO_PA_LNA_SELECT_PIN)
  // Set PA / LNA Select Pin: low for u.FL
  nrf_gpio_cfg_output(GPIO_PA_LNA_SELECT_PIN);
  nrf_gpio_pin_clear(GPIO_PA_LNA_SELECT_PIN);
#endif

  // Configure SoftDevice PA / LNA assist if required
#if defined(GPIO_PA_PIN) || defined(GPIO_LNA_PIN)
  
  static const uint32_t gpio_toggle_ch = 0;
  static const uint32_t ppi_set_ch = 0;
  static const uint32_t ppi_clr_ch = 1;
  
  varclr(&opt);
  
  // Common PA / LNA config
  // GPIOTE channel
  opt.common_opt.pa_lna.gpiote_ch_id = gpio_toggle_ch;
  // PPI channel for pin learing
  opt.common_opt.pa_lna.ppi_ch_id_clr = ppi_clr_ch;
  // PPI channel for pin setting
  opt.common_opt.pa_lna.ppi_ch_id_set = ppi_set_ch;
  
# if defined(GPIO_PA_PIN)
  // -- PA config --
  // Set the pin to be active high
  opt.common_opt.pa_lna.pa_cfg.active_high = GPIO_PA_PIN_ACTIVE_STATE;
  // Enable toggling
  opt.common_opt.pa_lna.pa_cfg.enable = 1;
  // The GPIO pin to toggle
  opt.common_opt.pa_lna.pa_cfg.gpio_pin = GPIO_PA_PIN;
# endif

# if defined(GPIO_LNA_PIN)
  // -- LNA config --
  // Set the pin to be active high
  opt.common_opt.pa_lna.lna_cfg.active_high = GPIO_LNA_PIN_ACTIVE_STATE;
  // Enable toggling
  opt.common_opt.pa_lna.lna_cfg.enable = 1;
  
  // The GPIO pin to toggle
  opt.common_opt.pa_lna.lna_cfg.gpio_pin = GPIO_LNA_PIN;
  sd_ble_opt_set(BLE_COMMON_OPT_PA_LNA, &opt);
# endif

  // Set TX power for scan responses
  sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_SCAN_INIT, 0, RADIO_TXPOWER_TXPOWER_Neg8dBm);
  
  // Set TX power for advertisements
  sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, 0, RADIO_TXPOWER_TXPOWER_Neg8dBm);
  // (Tx power setting for connections inherit the scan or advertising power setting)
  
#endif

  return NRF_SUCCESS;
}

/*------------------------------------------------------------------*/
/* SoftDevice Event handler
 *------------------------------------------------------------------*/
extern void ble_evt_dispatch(ble_evt_t *p_ble_evt);

// Process BLE event from SD
uint32_t proc_ble(void) {
  __ALIGN(4) uint8_t ev_buf[BLE_EVT_LEN_MAX(BLEGATT_ATT_MTU_MAX)];
  uint16_t ev_len = BLE_EVT_LEN_MAX(BLEGATT_ATT_MTU_MAX);

  // Init header
  ble_evt_t* evt = (ble_evt_t*) ev_buf;
  evt->header.evt_id = BLE_EVT_INVALID;

  // Get BLE Event
  uint32_t err = sd_ble_evt_get(ev_buf, &ev_len);

  // Handle valid event, ignore error
  if (NRF_SUCCESS == err) {
    switch (evt->header.evt_id) {
      case BLE_GAP_EVT_CONNECTED: {
        // Try to enable 2M phy,if phone allows it
        ble_gap_phys_t const phys =
        {
          .rx_phys = BLE_GAP_PHY_AUTO,
          .tx_phys = BLE_GAP_PHY_AUTO,
        };
        sd_ble_gap_phy_update(evt->evt.gap_evt.conn_handle, &phys);

        _ota_connected = true;

        // Remember someone connected to BLE
        _ota_was_connected = true;
        led_state(STATE_BLE_CONNECTED);
        break;
      }

      case BLE_GAP_EVT_DISCONNECTED:
        _ota_connected = false;
        led_state(STATE_BLE_DISCONNECTED);
        break;

      default:
        break;
    }

    // from dfu_transport_ble
    ble_evt_dispatch(evt);
  }

  return err;
}

// process SOC event from SD
uint32_t proc_soc(void) {
  uint32_t soc_evt = 0;
  uint32_t err = sd_evt_get(&soc_evt);

  if (NRF_SUCCESS == err) {
    pstorage_sys_event_handler(soc_evt);
  }

  return err;
}

void proc_sd_task(void* evt_data, uint16_t evt_size) {
  (void) evt_data;
  (void) evt_size;

  // process BLE and SOC until there is no more events
  while ((NRF_ERROR_NOT_FOUND != proc_ble()) || (NRF_ERROR_NOT_FOUND != proc_soc())) {
    // nothing
  }
}

void SD_EVT_IRQHandler(void) {
  // Use App Scheduler to defer handling code in non-isr context
  app_sched_event_put(NULL, 0, proc_sd_task);
}

//--------------------------------------------------------------------+
// Error Handler
//--------------------------------------------------------------------+
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info) {
  volatile uint32_t *ARM_CM_DHCSR = ((volatile uint32_t *)0xE000EDF0UL); /* Cortex M CoreDebug->DHCSR */
  if ((*ARM_CM_DHCSR) & 1UL) {
    __asm("BKPT #0\n");                                                  /* Only halt mcu if debugger is attached */
  }
  NVIC_SystemReset();
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
  app_error_fault_handler(0xDEADBEEF, 0, 0);
}


//--------------------------------------------------------------------+
// RTT printf retarget for Debug
//--------------------------------------------------------------------+
#ifdef CFG_DEBUG
#include "SEGGER_RTT.h"

__attribute__ ((used)) int _write (int fhdl, const void *buf, size_t count) {
  (void) fhdl;
  SEGGER_RTT_Write(0, (char*) buf, (int) count);
  return count;
}

#endif
