/*
 * Minimal startup file for nRF54L15 bootloader (Cortex-M33)
 *
 * Provides:
 * - Initial vector table (stack pointer + Reset_Handler + fault handlers)
 * - Reset_Handler: copies .data, zeroes .bss, calls main()
 * - Default/weak exception handlers
 */

    .syntax unified
    .cpu cortex-m33
    .thumb
    .fpu fpv5-sp-d16

    .global g_pfnVectors
    .global __isr_vector
    .extern __StackTop

/* ==========================================================================
 * Vector Table — placed at start of FLASH by linker (.isr_vector section)
 * ========================================================================== */

    .section .isr_vector, "a"
    .align 4
    .type g_pfnVectors, %object

__isr_vector:
g_pfnVectors:
    .word   __StackTop              /* 0: Initial Stack Pointer */
    .word   Reset_Handler           /* 1: Reset Handler */
    .word   NMI_Handler             /* 2: NMI Handler */
    .word   HardFault_Handler       /* 3: Hard Fault Handler */
    .word   MemManage_Handler       /* 4: MPU Fault Handler */
    .word   BusFault_Handler        /* 5: Bus Fault Handler */
    .word   UsageFault_Handler      /* 6: Usage Fault Handler */
    .word   SecureFault_Handler     /* 7: Secure Fault Handler */
    .word   0                       /* 8: Reserved */
    .word   0                       /* 9: Reserved */
    .word   0                       /* 10: Reserved */
    .word   SVC_Handler             /* 11: SVCall Handler */
    .word   DebugMon_Handler        /* 12: Debug Monitor Handler */
    .word   0                       /* 13: Reserved */
    .word   PendSV_Handler          /* 14: PendSV Handler */
    .word   SysTick_Handler         /* 15: SysTick Handler */

    /* External Interrupts — bootloader uses very few, fill with defaults */
    .rept 240
    .word   Default_Handler
    .endr

    .size g_pfnVectors, . - g_pfnVectors

/* ==========================================================================
 * Reset Handler
 * ========================================================================== */

    .section .text
    .align 2
    .thumb_func
    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:

    /* Copy .data from FLASH to RAM */
    ldr     r0, =__etext           /* source (end of .text in flash) */
    ldr     r1, =__data_start__    /* destination start */
    ldr     r2, =__data_end__      /* destination end */
.Lcopy_data:
    cmp     r1, r2
    bge     .Lcopy_done
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    b       .Lcopy_data
.Lcopy_done:

    /* Zero .bss */
    ldr     r0, =__bss_start__
    ldr     r1, =__bss_end__
    movs    r2, #0
.Lzero_bss:
    cmp     r0, r1
    bge     .Lbss_done
    str     r2, [r0], #4
    b       .Lzero_bss
.Lbss_done:

    /* Call SystemInit if available */
    ldr     r0, =SystemInit
    blx     r0

    /* Call main (bootloader entry, __START is defined as main) */
    ldr     r0, =main
    bx      r0

    .size Reset_Handler, . - Reset_Handler

/* ==========================================================================
 * Default exception / interrupt handlers (weak, overridable)
 * ========================================================================== */

    .align 2
    .thumb_func
    .weak Default_Handler
    .type Default_Handler, %function
Default_Handler:
    b       .
    .size Default_Handler, . - Default_Handler

    /* Weak aliases for all exception handlers */
    .weak   NMI_Handler
    .set    NMI_Handler, Default_Handler
    .weak   HardFault_Handler
    .set    HardFault_Handler, Default_Handler
    .weak   MemManage_Handler
    .set    MemManage_Handler, Default_Handler
    .weak   BusFault_Handler
    .set    BusFault_Handler, Default_Handler
    .weak   UsageFault_Handler
    .set    UsageFault_Handler, Default_Handler
    .weak   SecureFault_Handler
    .set    SecureFault_Handler, Default_Handler
    .weak   SVC_Handler
    .set    SVC_Handler, Default_Handler
    .weak   DebugMon_Handler
    .set    DebugMon_Handler, Default_Handler
    .weak   PendSV_Handler
    .set    PendSV_Handler, Default_Handler
    .weak   SysTick_Handler
    .set    SysTick_Handler, Default_Handler

    /* Provide a weak SystemInit in case nrfx doesn't supply one */
    .weak   SystemInit
    .thumb_func
    .type   SystemInit, %function
SystemInit:
    bx      lr
    .size   SystemInit, . - SystemInit

    .end
