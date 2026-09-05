/* Linker script to configure memory regions for nRF54LM20A bootloader
 *
 * nRF54LM20A memory map:
 *   RRAM: 2036 KB usable (0x00000000 - 0x001FD000)
 *   RAM:  512 KB          (0x20000000 - 0x2007FFFF, RAM + RAM2 contiguous)
 *
 * SoftDevice S145 v10.0.1 placement (s145_nrf54lm20_10.0.1_softdevice.hex):
 *   S145 sits at TOP of RRAM: base address 0x001DA800
 *   S145 size: 0x22400 (137 KB), ending at 0x001FCC00
 *   S145 RAM: 0x4780 - the bootloader/app RAM starts above that
 *
 * RRAM layout (bottom to top):
 *   0x00000000 - 0x00000FFF : MBR (4 KB)
 *   0x00001000 - 0x001C5FFF : Application code
 *   0x001C6000 - 0x001CFFFF : DFU app data reserved (40 KB = 10*4096)
 *   0x001D0000 - 0x001D7BFF : Bootloader (31 KB)
 *   0x001D7C00 - 0x001D7FFF : Bootloader config (1 KB)
 *   ... gap 0x001D8000 - 0x001DA7FF ...
 *   0x001DA800 - 0x001FCBFF : SoftDevice S145
 *   0x001FE000 - 0x001FEFFF : MBR params page
 *   0x001FF000 - 0x001FFFFF : Bootloader settings page
 *
 * Note: The bootloader is placed BELOW the SoftDevice in RRAM.
 *
 * The MBR params and bootloader settings pages mirror the addresses used by
 * the nRF54L05/L10/L15 scripts (top of the nominal RRAM window). As on those
 * parts, they fall above the last usable RRAM address (0x001FD000 here); this
 * is a pre-existing property of the family layout, not specific to LM20A.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Bootloader code: 31 KB below the bootloader config page */
  FLASH (rx) : ORIGIN = 0x1D0000, LENGTH = 0x7C00 /* 31 KB code + 1 KB padding */

  BOOTLOADER_CONFIG (r): ORIGIN = 0x1D7C00, LENGTH = 0x400 /* 1 KB */

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x1FE000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x1FF000, LENGTH = 0x1000

  /** RAM Region for bootloader. nRF54LM20A has 512 KB RAM.
   *  SoftDevice S145 v10.0.1 reserves 0x4780; we start at 0x4800 to keep the
   *  same app RAM base as the rest of the family. */
  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x20080000-0x20004800

  /* Location for double reset detection, no init */
  DBL_RESET (rwx) :  ORIGIN = 0x200047F8, LENGTH = 0x04

  /** Location of non initialized RAM. Non initialized RAM is used for exchanging bond information
   *  from application to bootloader when using buttonless DFU OTA. */
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

  /* Place the bootloader settings page in flash. */
  .bootloaderSettings(NOLOAD) :
  {

  } > BOOTLOADER_SETTINGS

  /* Write the bootloader address in UICR. */
  .uicrBootStartAddress :
  {
    KEEP(*(.uicrBootStartAddress))
  } > UICR_BOOTLOADER

  /* Place the mbr params page in flash. */
    .mbrParamsPage(NOLOAD) :
  {

  } > MBR_PARAMS_PAGE

  /* Write the bootloader address in UICR. */
  .uicrMbrParamsPageAddress :
  {
    KEEP(*(.uicrMbrParamsPageAddress))
  } > UICR_MBR_PARAM_PAGE

  .dbl_reset(NOLOAD) :
  {

  } > DBL_RESET

  /* No init RAM section in bootloader. Used for bond information exchange. */
  .noinit(NOLOAD) :
  {

  } > NOINIT

  /* other placements follow here... */
}

INCLUDE "nrf_common.ld"
