#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runs the SoftDevice reset handler once; required before any sd_* SVC. Called at the start of main().
void sd_isr_boot_init(void);
// Route the SoftDevice-owned interrupts to the s145 image; call before sd_softdevice_enable().
void sd_isr_forwarding_enable(void);
void sd_isr_forwarding_disable(void);

#ifdef __cplusplus
}
#endif
