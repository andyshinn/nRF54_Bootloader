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

#include "boards.h"
#include "nrf_pwm.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "nrfy_grtc.h"

#define SCHED_MAX_EVENT_DATA_SIZE           sizeof(app_timer_event_t)        /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                    30                               /**< Maximum number of events in the scheduler queue. */

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

static uint32_t _systick_count = 0;
void SysTick_Handler(void) {
  _systick_count++;
  led_tick();
}

#if defined(BUTTON_DFU) || defined(BUTTON_DFU_OTA)
void button_init(uint32_t pin) {
  if (BUTTON_PULL == NRF_GPIO_PIN_PULLDOWN) {
    nrf_gpio_cfg_sense_input(pin, BUTTON_PULL, NRF_GPIO_PIN_SENSE_HIGH);
  } else {
    nrf_gpio_cfg_sense_input(pin, BUTTON_PULL, NRF_GPIO_PIN_SENSE_LOW);
  }
}

// At power-on the internal pull (~13k) may still be charging the button net when the pin is
// first read, so it looks pressed. Only report a press if the pin stays active for the whole
// window; a released button returns as soon as the pin settles.
#ifndef BUTTON_SETTLE_MS
#define BUTTON_SETTLE_MS      50
#endif

bool button_pressed(uint32_t pin) {
  uint32_t const active_state = (BUTTON_PULL == NRF_GPIO_PIN_PULLDOWN ? 1 : 0);
  for (uint32_t i = 0; i < BUTTON_SETTLE_MS * 10; i++) {
    if (nrf_gpio_pin_read(pin) != active_state) return false;
    NRFX_DELAY_US(100);
  }
  return true;
}
#endif

// This is declared so that a board specific init can be called from here.
void __attribute__((weak)) board_init2(void) {}

void board_init(void) {
  NRF_CLOCK->LFCLK.SRC = (CLOCK_LFCLK_SRC_SRC_LFRC << CLOCK_LFCLK_SRC_SRC_Pos);
  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  // sd_softdevice_enable() requires the GRTC SYSCOUNTER running with AUTOEN; the counter survives soft resets
  if (!nrf_grtc_sys_counter_check(NRF_GRTC)) {
    nrfy_grtc_prepare(NRF_GRTC, true);
    nrfy_grtc_sys_counter_start(NRF_GRTC, true);
  }
  nrf_grtc_sys_counter_auto_mode_set(NRF_GRTC, true);

#ifdef BUTTON_DFU
  button_init(BUTTON_DFU);
#endif
#ifdef BUTTON_DFU_OTA
  button_init(BUTTON_DFU_OTA);
#endif

#if LEDS_NUMBER > 0
  // use PMW0 for LED RED
  led_pwm_init(LED_PRIMARY, LED_PRIMARY_PIN);
  #if LEDS_NUMBER > 1
  led_pwm_init(LED_SECONDARY, LED_SECONDARY_PIN);
  #endif
#endif

  /* nRF54L DC-DC is configured via REGULATORS peripheral, not NRF_POWER */

  // Make sure any custom inits are performed
  board_init2();

  // When board is supplied on VDDH (and not VDD), this specifies what voltage the GPIO should run at
  // and what voltage is output at VDD. The default (0xffffffff) is 1.8V; typically you'll want
  //     #define UICR_REGOUT0_VALUE UICR_REGOUT0_VOUT_3V3
  // in board.h when using that power configuration.
  /* nRF54L uses RRAMC instead of NVMC, and has different UICR layout.
   * REGOUT0 voltage configuration is not applicable to nRF54L. */

  // Init scheduler
  APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

  // Init app timer (use RTC1)
  app_timer_init();

  // Configure Systick for led blinky
  NVIC_SetPriority(SysTick_IRQn, 7);
  SysTick_Config(SystemCoreClock / 1000);
}

// Actions at the end of board_teardown.
void __attribute__((weak)) board_teardown2(void) {}

void board_teardown(void) {
  // Disable systick, turn off LEDs
  SysTick->CTRL = 0;

  // Disable and reset PWM for LEDs
#if LEDS_NUMBER > 0
  led_pwm_teardown();
#endif

  // Stop TIMER21 used by app_timer (nRF54L has no RTC; uses hardware timer)
  app_timer_stop_all();

  // Stop the LF clock so the application can pick its own source
  NRF_CLOCK->TASKS_LFCLKSTOP = 1;

  // make sure all pins are back in reset state
  // NUMBER_OF_PINS is defined in nrf_gpio.h
  for (int i = 0; i < NUMBER_OF_PINS; ++i) {
    nrf_gpio_cfg_default(i);
  }

  // board specific teardown actions
  board_teardown2();
}

//--------------------------------------------------------------------+
// LED Indicator
//--------------------------------------------------------------------+
static uint16_t pwm_dummy_seq;

void pwm_teardown(NRF_PWM_Type* pwm) {
  nrf_pwm_disable(pwm);

  nrf_pwm_pins_set(pwm, (uint32_t[]){
      NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED,
      NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED});

  nrf_pwm_configure(pwm, NRF_PWM_CLK_16MHz, NRF_PWM_MODE_UP, 0x3FF);
  nrf_pwm_decoder_set(pwm, PWM_DECODER_LOAD_Common, PWM_DECODER_MODE_RefreshCount);
  nrf_pwm_loop_set(pwm, 0);
  nrf_pwm_seq_ptr_set(pwm, 0, &pwm_dummy_seq);
  nrf_pwm_seq_cnt_set(pwm, 0, 0);
}

#define PWM_CH_NUM 4
static uint16_t led_duty_cycles[PWM_CH_NUM] = {0};

#if LEDS_NUMBER > PWM_CH_NUM
#error "Only 4 concurrent status LEDs are supported."
#endif

void led_pwm_init(uint32_t led_index, uint32_t led_pin) {
  NRF_PWM_Type* pwm = NRF_PWM20;

  nrf_pwm_disable(pwm);

  nrf_gpio_cfg_output(led_pin);
  nrf_gpio_pin_write(led_pin, 1 - LED_STATE_ON);

  // Set only the requested output pin; nrf_pwm_pins_set() sets all 4 at once
  // so we use direct register access for per-channel pin assignment
  pwm->PSEL.OUT[led_index] = led_pin;

  nrf_pwm_configure(pwm, NRF_PWM_CLK_1MHz, NRF_PWM_MODE_UP, 0xff);
  nrf_pwm_decoder_set(pwm, PWM_DECODER_LOAD_Individual, PWM_DECODER_MODE_RefreshCount);
  nrf_pwm_loop_set(pwm, 0);

  nrf_pwm_seq_ptr_set(pwm, 0, led_duty_cycles);
  nrf_pwm_seq_cnt_set(pwm, 0, 4); // Individual mode --> count must be 4
  nrf_pwm_seq_refresh_set(pwm, 0, 0);
  nrf_pwm_seq_end_delay_set(pwm, 0, 0);

  nrf_pwm_enable(pwm);

  nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_SEQEND0);
}

void led_pwm_teardown(void) {
  pwm_teardown(NRF_PWM20);
}

void led_pwm_duty_cycle(uint32_t led_index, uint16_t duty_cycle) {
  led_duty_cycles[led_index] = duty_cycle;
  nrf_pwm_event_clear(NRF_PWM20, NRF_PWM_EVENT_SEQEND0);
  nrf_pwm_task_trigger(NRF_PWM20, NRF_PWM_TASK_SEQSTART0);
}

static uint32_t primary_cycle_length;
#ifdef LED_SECONDARY_PIN
static uint32_t secondary_cycle_length;
#endif

void led_tick(void) {
  uint32_t millis = _systick_count;

  uint32_t cycle = millis % primary_cycle_length;
  uint32_t half_cycle = primary_cycle_length / 2;
  if (cycle > half_cycle) {
    cycle = primary_cycle_length - cycle;
  }
  uint16_t duty_cycle = 0x4f * cycle / half_cycle;
  #if LED_STATE_ON == 1
  duty_cycle = 0xff - duty_cycle;
  #endif
  led_pwm_duty_cycle(LED_PRIMARY, duty_cycle);

  #ifdef LED_SECONDARY_PIN
  cycle = millis % secondary_cycle_length;
  half_cycle = secondary_cycle_length / 2;
  if (cycle > half_cycle) {
      cycle = secondary_cycle_length - cycle;
  }
  duty_cycle = 0x8f * cycle / half_cycle;
  #if LED_STATE_ON == 1
  duty_cycle = 0xff - duty_cycle;
  #endif
  led_pwm_duty_cycle(LED_SECONDARY, duty_cycle);
  #endif
}

void led_state(uint32_t state) {
  switch (state) {
    case STATE_USB_MOUNTED:
    case STATE_WRITING_FINISHED:
      primary_cycle_length = 3000;
      break;

    case STATE_BOOTLOADER_STARTED:
    case STATE_USB_UNMOUNTED:
      primary_cycle_length = 300;
      break;

    case STATE_WRITING_STARTED:
      primary_cycle_length = 100;
      break;

    case STATE_BLE_CONNECTED:
      #ifdef LED_SECONDARY_PIN
      secondary_cycle_length = 3000;
      #else
      primary_cycle_length = 3000;
      #endif
      break;

    case STATE_BLE_DISCONNECTED:
      #ifdef LED_SECONDARY_PIN
      secondary_cycle_length = 300;
      #else
      primary_cycle_length = 300;
      #endif
      break;

    default:
      break;
  }
}
