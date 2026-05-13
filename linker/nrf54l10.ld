/* Linker script to configure memory regions for nRF54L10 bootloader
 *
 * nRF54L10 memory map:
 *   RRAM: 1 MB   (0x00000000 - 0x000FFFFF)
 *   RAM:  192 KB  (0x20000000 - 0x2002FFFF)
 *
 * SoftDevice S145 v9.0.0 placement:
 *   S145 sits at TOP of RRAM: base address 0x000D8C00
 *   S145 size: 0x24000 (144 KB)
 *   S145 RAM: 18 KB (0x4800) - static 6KB + dynamic 12KB
 *
 * RRAM layout (bottom to top):
 *   0x00000000 - 0x00000FFF : MBR (4 KB)
 *   0x00001000 - 0x000CDFFF : Application code (~819 KB)
 *   0x000CE000 - 0x000CFFFF : DFU app data reserved (8 KB = 2*4096)
 *   0x000D0000 - 0x000D87FF : Bootloader (34 KB code)
 *   0x000D8800 - 0x000D8BFF : Bootloader config (1 KB)
 *   0x000D8C00 - 0x000FCBFF : SoftDevice S145
 *   0x000FE000 - 0x000FEFFF : MBR params page
 *   0x000FF000 - 0x000FFFFF : Bootloader settings page
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Bootloader code: ~34KB below S145 base */
  FLASH (rx) : ORIGIN = 0x0D0000, LENGTH = 0x8800

  BOOTLOADER_CONFIG (r): ORIGIN = 0x0D8800, LENGTH = 0x400

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x0FE000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x0FF000, LENGTH = 0x1000

  /** RAM Region for bootloader. nRF54L10 has 192KB RAM.
   *  SoftDevice S145 uses 18 KB (0x4800): 6 KB static + 12 KB dynamic
   *  App/bootloader RAM starts at 0x20004800 */
  RAM (rwx) :  ORIGIN = 0x20004800, LENGTH = 0x20030000-0x20004800

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
