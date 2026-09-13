#include "sd_isr.h"
#include "nrf.h"
#include "dfu_types.h"
#include <stdbool.h>

#define SD_ISR_MAGIC 0x47F34BC1u

// SVCs are always forwarded (sd_softdevice_is_enabled() is used before BLE init), IRQs only once enabled.
uint32_t sd_isr_forward_base = SOFTDEVICE_REGION_START;
uint32_t sd_isr_forward_enabled = 0;
static bool sd_reset_done = false;

extern void sd_isr_call_reset_handler(void);

void sd_isr_boot_init(void)
{
    if (sd_reset_done) return;
    sd_isr_call_reset_handler();
    sd_reset_done = true;
}

void sd_isr_forwarding_enable(void)
{
    // Priorities the SoftDevice expects for its interrupts (3 priority bits).
    NVIC_SetPriority(RADIO_0_IRQn, 0);
    NVIC_SetPriority(TIMER10_IRQn, 0);
    NVIC_SetPriority(GRTC_3_IRQn, 0);
    NVIC_SetPriority(AAR00_CCM00_IRQn, 4);
    NVIC_SetPriority(CLOCK_POWER_IRQn, 4);
    NVIC_SetPriority(ECB00_IRQn, 4);
    NVIC_SetPriority(SWI00_IRQn, 4);
    NVIC_SetPriority(SVCall_IRQn, 4);

    sd_isr_boot_init();
    sd_isr_forward_enabled = SD_ISR_MAGIC;
}

void sd_isr_forwarding_disable(void)
{
    sd_isr_forward_enabled = 0;
}
