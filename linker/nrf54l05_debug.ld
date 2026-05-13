/* Linker script for nRF54L05 - DEBUG version
 * See nrf54l05.ld for full memory map documentation.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Debug bootloader: starts earlier for extra debug code */
  FLASH (rx) : ORIGIN = 0x048000, LENGTH = 0x10800

  BOOTLOADER_CONFIG (r): ORIGIN = 0x058800, LENGTH = 0x400

  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x07E000, LENGTH = 0x1000
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x07F000, LENGTH = 0x1000

  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x20018000-0x20004800
  DBL_RESET (rwx) :  ORIGIN = 0x200047F8, LENGTH = 0x04
  NOINIT (rwx) :  ORIGIN = 0x200047FC, LENGTH = 0x04

  UICR_BOOTLOADER (r) : ORIGIN = 0x10001014, LENGTH = 0x04
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
