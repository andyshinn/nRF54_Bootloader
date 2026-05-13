/* Linker script to configure memory regions for nRF54L15 bootloader
 *
 * nRF54L15 memory map:
 *   RRAM: 1.5 MB (0x00000000 - 0x0017FFFF)
 *   RAM:  256 KB  (0x20000000 - 0x2003FFFF)
 *
 * SoftDevice S145 v9.0.0 placement:
 *   S145 sits at TOP of RRAM: base address 0x00158C00
 *   S145 size: 0x24000 (144 KB)
 *   S145 RAM: 18 KB (0x4800) - static 6KB + dynamic 12KB
 *
 * RRAM layout (bottom to top):
 *   0x00000000 - 0x00000FFF : MBR (4 KB)
 *   0x00001000 - 0x0014DFFF : Application code (~1335 KB)
 *   0x0014E000 - 0x00155FFF : DFU app data reserved (40 KB = 10*4096)
 *   0x00156000 - 0x00157FFF : Bootloader config + settings (see below)
 *   0x00158000 - 0x0015FFFF : Bootloader (32 KB)
 *   ... gap ...
 *   0x00158C00 - 0x0017BFFF : SoftDevice S145
 *   0x0017E000 - 0x0017EFFF : MBR params page
 *   0x0017F000 - 0x0017FFFF : Bootloader settings page
 *
 * Note: The bootloader is placed BELOW the SoftDevice in RRAM.
 * Bootloader region: 0x00150000 - 0x00158000 (32 KB)
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Bootloader code: 32KB below S145 base */
  FLASH (rx) : ORIGIN = 0x150000, LENGTH = 0x7C00 /* 31 KB code + 1 KB padding */

  BOOTLOADER_CONFIG (r): ORIGIN = 0x157C00, LENGTH = 0x400 /* 1 KB */

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x17E000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x17F000, LENGTH = 0x1000

  /** RAM Region for bootloader. nRF54L15 has 256KB RAM.
   *  SoftDevice S145 uses 18 KB (0x4800): 6 KB static + 12 KB dynamic
   *  App/bootloader RAM starts at 0x20004800 */
  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x20040000-0x20004800

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
