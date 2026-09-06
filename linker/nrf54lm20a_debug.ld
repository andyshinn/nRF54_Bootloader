/* Linker script for nRF54LM20A - DEBUG version
 *
 * Debug build starts the bootloader earlier to allow more space for
 * debugging code (SEGGER RTT, etc.).
 *
 * See nrf54lm20a.ld for full memory map documentation.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /** Reset-vector stub — see nrf54lm20a.ld. */
  BOOT_STUB (rx) : ORIGIN = 0x0, LENGTH = 0x100

  /* Debug bootloader: 63 KB below the bootloader config page */
  FLASH (rx) : ORIGIN = 0x1C8000, LENGTH = 0x00FC00 /* ~63 KB code + 1 KB padding */

  BOOTLOADER_CONFIG (r): ORIGIN = 0x1D7C00, LENGTH = 0x400 /* 1 KB */

  /** Location of mbr params page in flash. See nrf54lm20a.ld for why these
   *  are not at the top of the RRAM window like the nRF54L05/L10/L15 scripts. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x1D8000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x1D9000, LENGTH = 0x1000

  /** RAM Region for bootloader. Top of RAM is inaccessible; Nordic ends app
   *  RAM at 0x2007FCC0 (see nrf54lm20a.ld). */
  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x2007FCC0-0x20004800

  /* Location for double reset detection, no init */
  DBL_RESET (rwx) :  ORIGIN = 0x200047F8, LENGTH = 0x04

  /** Location of non initialized RAM. */
  NOINIT (rwx) :  ORIGIN = 0x200047FC, LENGTH = 0x04


}

SECTIONS
{
  /* Base of the bootloader's own vector table, for src/boot_stub.S. */
  __bootloader_vectors = ORIGIN(FLASH);

  .fs_data_out ALIGN(4):
  {
    PROVIDE( __start_fs_data = .);
    KEEP(*(fs_data))
    PROVIDE( __stop_fs_data = .);
  } = 0

  .boot_stub :
  {
    KEEP(*(.boot_stub))
  } > BOOT_STUB

  .bootloaderConfig :
  {
    KEEP(*(.bootloaderConfig))
  } > BOOTLOADER_CONFIG

  .bootloaderSettings(NOLOAD) :
  {

  } > BOOTLOADER_SETTINGS


  .mbrParamsPage(NOLOAD) :
  {

  } > MBR_PARAMS_PAGE


  .dbl_reset(NOLOAD) :
  {

  } > DBL_RESET

  .noinit(NOLOAD) :
  {

  } > NOINIT
}

INCLUDE "nrf_common.ld"
