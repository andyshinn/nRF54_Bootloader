/*
 * Reset-vector stub for nRF54L ("MBR equivalent").
 *
 * nRF54L has no MBR. Nordic's own model puts the reset-vector owner at
 * address 0 (in sdk-nrf-bm the application is slot0 at 0x00000000) and the
 * SoftDevice at the top of RRAM. This port inherited the nRF52 layout, where
 * an MBR at 0 reads UICR.BOOTLOADERADDR and forwards to the bootloader — but
 * nRF54L has neither that MBR nor a UICR.BOOTLOADERADDR field, so nothing
 * occupied 0 and the core fetched SP/PC of 0xFFFFFFFF at reset and locked up.
 *
 * This stub lives in the 4 KB page already reserved below the application
 * (MBR_SIZE), so it costs no usable flash:
 *
 *   0x00000000  initial SP   (loaded by hardware at reset)
 *   0x00000004  entry        (loaded by hardware at reset -> boot_stub_entry)
 *
 * boot_stub_entry points VTOR at the bootloader's own vector table before
 * jumping, so interrupts are correctly vectored from the very first
 * instruction. That matters because check_dfu_mode() enables interrupts long
 * before ble_stack_init() sets VTOR at main.c.
 *
 * __bootloader_vectors is provided by the linker script as ORIGIN(FLASH), so
 * this works unchanged for both the release and debug layouts.
 *
 * SP and the entry point are read from the bootloader's vector table at
 * runtime rather than being linked in, so the stub stays valid when the
 * bootloader itself is replaced by a DFU update and its Reset_Handler moves.
 */

    .syntax unified
    .arch   armv8-m.main

    .section .boot_stub, "ax"
    .align  2
    .globl  __boot_stub
__boot_stub:
    .long   __StackTop                  /* [0x00] initial SP  */
    .long   boot_stub_entry + 1         /* [0x04] entry, thumb */

    .thumb_func
    .globl  boot_stub_entry
boot_stub_entry:
    ldr     r0, =__bootloader_vectors    /* bootloader vector table */
    ldr     r1, =0xE000ED08              /* SCB->VTOR              */
    str     r0, [r1]
    dsb
    isb
    ldr     r1, [r0]                     /* vectors[0] = SP        */
    mov     sp, r1
    ldr     r1, [r0, #4]                 /* vectors[1] = Reset     */
    bx      r1
    .size   boot_stub_entry, . - boot_stub_entry
