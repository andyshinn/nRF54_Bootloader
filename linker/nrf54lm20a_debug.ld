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
  /* Debug bootloader: 63 KB below the bootloader config page */
  FLASH (rx) : ORIGIN = 0x1C8000, LENGTH = 0x00FC00 /* ~63 KB code + 1 KB padding */

  BOOTLOADER_CONFIG (r): ORIGIN = 0x1D7C00, LENGTH = 0x400 /* 1 KB */

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x1FE000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x1FF000, LENGTH = 0x1000

  /** RAM Region for bootloader. nRF54LM20A has 512 KB RAM. */
  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x20080000-0x20004800

  /* Location for double reset detection, no init */
  DBL_RESET (rwx) :  ORIGIN = 0x200047F8, LENGTH = 0x04

  /** Location of non initialized RAM. */
  NOINIT (rwx) :  ORIGIN = 0x200047FC, LENGTH = 0x04

  /** Location in UICR where bootloader start address is stored. */
  UICR_BOOTLOADER (r) : ORIGIN = 0x10001014, LENGTH = 0x04

  /** Location in UICR where mbr params page address is stored. */
  UICR_MBR_PARAM_PAGE(r) : ORIGIN = 0x10001018, LENGTH = 0x04
}

SECTIONS
{
  .fs_data_out ALIGN(4):
  {
    PROVIDE( __start_fs_data = .);
    KEEP(*(fs_data))
    PROVIDE( __stop_fs_data = .);
  } = 0

  .bootloaderConfig :
  {
    KEEP(*(.bootloaderConfig))
  } > BOOTLOADER_CONFIG

  .bootloaderSettings(NOLOAD) :
  {

  } > BOOTLOADER_SETTINGS

  .uicrBootStartAddress :
  {
    KEEP(*(.uicrBootStartAddress))
  } > UICR_BOOTLOADER

  .mbrParamsPage(NOLOAD) :
  {

  } > MBR_PARAMS_PAGE

  .uicrMbrParamsPageAddress :
  {
    KEEP(*(.uicrMbrParamsPageAddress))
  } > UICR_MBR_PARAM_PAGE

  .dbl_reset(NOLOAD) :
  {

  } > DBL_RESET

  .noinit(NOLOAD) :
  {

  } > NOINIT
}

INCLUDE "nrf_common.ld"
