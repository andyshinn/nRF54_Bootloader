#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

// Power
#define NRFX_POWER_ENABLED  1
#define NRFX_POWER_DEFAULT_CONFIG_IRQ_PRIORITY  7

#define NRFX_CLOCK_ENABLED  0

// nRF54L uses RRAMC, not NVMC
// NRFX_NVMC is not available on nRF54L

#define NRFX_PRS_ENABLED    0

// PWM
#define NRFX_PWM_ENABLED    0
#define NRFX_PWM0_ENABLED   0
#define NRFX_PWM1_ENABLED   0
#define NRFX_PWM2_ENABLED   0
#define NRFX_PWM3_ENABLED   0

#endif
