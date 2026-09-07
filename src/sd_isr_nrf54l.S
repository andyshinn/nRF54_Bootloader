/*
 * SoftDevice exception forwarding for nRF54L.
 *
 * nRF52 put an MBR at address 0 that owned the real vector table and split
 * exceptions between the SoftDevice, the bootloader and the application, which
 * is why the SDK11 code this bootloader came from never had to think about it.
 * nRF54L has no MBR. S145 instead publishes a small table of handler addresses
 * at the SoftDevice base address, and whoever owns the vector table -- here the
 * bootloader -- has to forward the corresponding exceptions to it. From
 * nrf_sd_isr.h:
 *
 *     The SoftDevice interrupt vector table contains only the addresses of the
 *     interrupt handlers required by the SoftDevice. The table is located at
 *     the SoftDevice base address. When the SoftDevice is enabled, the
 *     application must forward the interrupts corresponding to the defined
 *     offsets to the SoftDevice.
 *
 * Without this the SoftDevice cannot run at all: sd_softdevice_enable() is
 * itself an SVC, so it lands in the vector table's SVC_Handler, and before this
 * file existed that was the bootloader's own DFU handler, which answered
 * NRF_ERROR_SVC_HANDLER_MISSING to every SoftDevice call.
 *
 * SOFTDEVICE_BASE_ADDRESS is read out of the SoftDevice hex at configure time
 * by tools/sd_base_addr.py, so it always matches the blob being shipped.
 *
 * The handlers below are strong definitions and override the weak aliases to
 * Default_Handler in the MDK startup file. Peripherals the SoftDevice reserves
 * are listed in nrf_sd_def.h; note SWI00 is the SoftDevice's own and is
 * forwarded here, while SWI01 (SD_EVT_IRQn) and SWI02 (RADIO_NOTIFICATION_IRQn)
 * are raised by the SoftDevice for us to handle and must not be forwarded.
 */

#include "nrf_sd_isr.h"

#ifndef SOFTDEVICE_BASE_ADDRESS
#error "SOFTDEVICE_BASE_ADDRESS is not defined (see CMakeLists.txt)"
#endif

/* SVC numbers 0x00-0x0F are the application's -- for this image, the DFU SVCs
 * in src/dfu_ble_svc.c. Everything at or above this belongs to the SoftDevice.
 * See the SVCALL() documentation in nrf_svc.h. */
#define SD_SVC_FIRST    0x10

    .syntax unified
    .arch   armv8-m.main
    .text

/* Branch to the handler whose address the SoftDevice publishes at
 * SOFTDEVICE_BASE_ADDRESS + \offset, leaving the exception stack frame and
 * LR (EXC_RETURN) untouched so the SoftDevice returns from the exception
 * itself. Clobbering r0 is safe and is what Nordic's own example does: it is
 * caller-saved and has already been stacked by the exception entry. */
    .macro  SD_FORWARD name, offset
    .thumb_func
    .globl  \name
    .type   \name, %function
\name:
    ldr     r0, =(SOFTDEVICE_BASE_ADDRESS + \offset)
    ldr     r0, [r0]
    bx      r0
    .size   \name, . - \name
    .ltorg
    .endm

/*
 * SVC dispatch.
 *
 * Decodes the SVC number out of the instruction that raised the exception and
 * either forwards to the SoftDevice or falls through to the bootloader's own
 * C_SVC_Handler(svc_num, p_svc_args) in src/dfu_ble_svc.c. C_SVC_Handler is
 * entered with LR still holding EXC_RETURN, so its return is the exception
 * return -- that is how the SDK11 version of this handler worked too.
 */
    .thumb_func
    .globl  SVC_Handler
    .type   SVC_Handler, %function
SVC_Handler:
    /* EXC_RETURN bit 2 clear means the pre-exception frame is on the MSP, set
     * means the PSP. The SDK11 handler compared LR against 0xFFFFFFFD outright,
     * which only recognises one of the several EXC_RETURN values Armv8-M can
     * produce -- with a floating-point frame, or in secure state, that compare
     * silently picks the wrong stack. */
    tst     lr, #4
    beq     1f
    mrs     r1, psp
    b       2f
1:
    mrs     r1, msp
2:
    /* r1 now points at the stacked {r0,r1,r2,r3,r12,lr,pc,xpsr}. The stacked PC
     * is the instruction after the SVC, and the SVC number is the low byte of
     * the SVC instruction itself. */
    ldr     r0, [r1, #24]
    subs    r0, r0, #2
    ldrb    r0, [r0]

    cmp     r0, #SD_SVC_FIRST
    bhs     3f

    /* Bootloader SVC: r0 = number, r1 = stacked frame, matching the
     * C_SVC_Handler prototype. */
    b       C_SVC_Handler

3:
    ldr     r0, =(SOFTDEVICE_BASE_ADDRESS + NRF_SD_ISR_OFFSET_SVC)
    ldr     r0, [r0]
    bx      r0
    .size   SVC_Handler, . - SVC_Handler
    .ltorg

/* The rest of the table. RESET and HARDFAULT are deliberately not forwarded:
 * the bootloader owns its own reset path (src/boot_stub.S) and wants its own
 * fault handler for diagnostics. */
    SD_FORWARD SWI00_IRQHandler,       NRF_SD_ISR_OFFSET_SWI00
    SD_FORWARD AAR00_CCM00_IRQHandler, NRF_SD_ISR_OFFSET_AAR00_CCM00
    SD_FORWARD ECB00_IRQHandler,       NRF_SD_ISR_OFFSET_ECB00
    SD_FORWARD TIMER10_IRQHandler,     NRF_SD_ISR_OFFSET_TIMER10
    SD_FORWARD RADIO_0_IRQHandler,     NRF_SD_ISR_OFFSET_RADIO_0
    SD_FORWARD GRTC_3_IRQHandler,      NRF_SD_ISR_OFFSET_GRTC_3
    SD_FORWARD CLOCK_POWER_IRQHandler, NRF_SD_ISR_OFFSET_CLOCK_POWER

    .end
