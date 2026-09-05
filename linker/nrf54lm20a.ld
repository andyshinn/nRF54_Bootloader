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
 *   0x00001000 - 0x001C6FFF : Application code
 *   0x001C7000 - 0x001CFFFF : DFU app data reserved (36 KB = 9*4096)
 *                             (28 KB InternalFS 0x1C7000-0x1CDFFF + 8 KB gap)
 *   0x001D0000 - 0x001D7BFF : Bootloader (31 KB)
 *   0x001D7C00 - 0x001D7FFF : Bootloader config (1 KB)
 *   0x001D8000 - 0x001D8FFF : MBR params page
 *   0x001D9000 - 0x001D9FFF : Bootloader settings page
 *   ... gap 0x001DA000 - 0x001DA7FF ...
 *   0x001DA800 - 0x001FCBFF : SoftDevice S145
 *   0x001FCC00 - 0x001FCFFF : unused tail of the last RRAM page
 *
 * Note: The bootloader is placed BELOW the SoftDevice in RRAM.
 *
 * The MBR params and bootloader settings pages deliberately do NOT mirror the
 * nRF54L05/L10/L15 scripts, which put them at the top of the nominal RRAM
 * window. On this part that would be 0x001FE000/0x001FF000, both past the last
 * usable RRAM address: the MDK gives NRF_MEMORY_FLASH_SIZE = 0x001FD000
 * (nrf54lm20a_xxaa_application_memory.h) and the SVD says "2036 KByte RRAM".
 * bootloader_app_is_valid() reads bank_0 out of the settings page, so a page
 * that is not physically backed would leave the app permanently invalid and
 * the board stuck in DFU. They are placed instead in the two free, page
 * aligned slots between the bootloader config page and the SoftDevice base.
 *
 * Keep in sync with BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS and
 * BOOTLOADER_SETTINGS_ADDRESS in sdk11 dfu_types.h, and with
 * bootloader.settings_addr in the platform's boards/xiao_nrf54lm20a.json.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Bootloader code: 31 KB below the bootloader config page */
  FLASH (rx) : ORIGIN = 0x1D0000, LENGTH = 0x7C00 /* 31 KB code + 1 KB padding */

  BOOTLOADER_CONFIG (r): ORIGIN = 0x1D7C00, LENGTH = 0x400 /* 1 KB */

  /** Location of mbr params page in flash. */
  MBR_PARAMS_PAGE (rw) : ORIGIN = 0x1D8000, LENGTH = 0x1000

  /** Location of bootloader setting in flash. */
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x1D9000, LENGTH = 0x1000

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
