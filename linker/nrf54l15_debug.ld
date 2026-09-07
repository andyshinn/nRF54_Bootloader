/* Linker script to configure memory regions for nRF54L15 - DEBUG version
 *
 * Debug build starts the bootloader earlier to allow more space for
 * debugging code (SEGGER RTT, etc.).
 *
 * See nrf54l15.ld for full memory map documentation.
 */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* Debug bootloader: 64KB below S145 base for extra debug code */
  FLASH (rx) : ORIGIN = 0x148000, LENGTH = 0x00FC00 /* ~63 KB code + 1 KB padding */

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

  /* There is deliberately no NOINIT region. It used to be declared here as a
   * 4-byte window at 0x200047FC, which could never hold the ~62 bytes of
   * .noinit the bootloader actually has, and the .noinit output section that
   * targeted it had no input wildcard, so the block was silently orphaned.
   * .noinit is now placed at ORIGIN(RAM) by the SECTIONS command below. */

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
    KEEP(*(.bootloaderSettings))
  } > BOOTLOADER_SETTINGS

  /* Write the bootloader address in UICR. */
  .uicrBootStartAddress :
  {
    KEEP(*(.uicrBootStartAddress))
  } > UICR_BOOTLOADER

  /* Place the mbr params page in flash. */
  .mbrParamsPage(NOLOAD) :
  {
    KEEP(*(.mbrParamsPage))
  } > MBR_PARAMS_PAGE

  /* Write the bootloader address in UICR. */
  .uicrMbrParamsPageAddress :
  {
    KEEP(*(.uicrMbrParamsPageAddress))
  } > UICR_MBR_PARAM_PAGE

  /* Location for double reset detection, no init. src/main.c reaches the word
   * through __dbl_reset_mem rather than a literal address, so the two cannot
   * drift apart. Nothing is placed in DBL_RESET, which is the point: the
   * region exists so no output section is ever allocated over the word. */
  __dbl_reset_mem = ORIGIN(DBL_RESET);

  .dbl_reset(NOLOAD) :
  {

  } > DBL_RESET

  /* Non initialized RAM section in bootloader. Used for bond information
   * exchange with the application across a soft reset.
   *
   * This has to be captured here, in the first SECTIONS command, so that it
   * lands at ORIGIN(RAM) and its address is fixed. The MDK's nrf_common.ld
   * (which is what the INCLUDE below resolves to) has its own generic .noinit
   * rule that would otherwise take these inputs and stack them on top of
   * .data, where the address moves with every build. Without any wildcard at
   * all -- as this section used to be -- the inputs were orphaned and ld chose
   * the address on its own. */
  .noinit(NOLOAD) :
  {
    /* The bond window the application hands over across a soft reset. Both
     * halves are pinned by name and in order: the application writes one
     * 62-byte block -- 60 bytes of dfu_ble_peer_data_t then a uint16 CRC -- at
     * its own ORIGIN(RAM), and src/dfu_ble_svc.c's two variables have to land
     * in exactly that order. A bare *(.noinit) wildcard leaves the order to the
     * compiler, and GCC emitted the CRC first, so the bootloader checksummed
     * the wrong bytes and fell back to non-bonded advertising without saying
     * so. The assertions below make a future reordering a link error. */
    __bootloader_peer_data = .;
    KEEP(*(.noinit.peer_data))
    __bootloader_peer_data_crc = .;
    KEEP(*(.noinit.peer_data_crc))
    KEEP(*(.noinit .noinit.*))
    KEEP(*(.no_init .no_init.*))
  } > RAM

  ASSERT(__bootloader_peer_data == ORIGIN(RAM),
         "DFU bond window must start at ORIGIN(RAM)")
  ASSERT(__bootloader_peer_data_crc - __bootloader_peer_data == 60,
         "DFU bond CRC must sit 60 bytes into the window, right after the peer data")

  /* other placements follow here... */
}

/* Resolves to lib/nrfx/bsp/stable/mdk/nrf_common.ld, which is on the link line
 * as -L<nrfx>/bsp/stable/mdk. That is deliberate: the MDK startup code defines
 * __data_start / __sdata_start / __tbss_start / __fast_load_start and friends,
 * and only the MDK script provides them. This repo used to also ship a
 * linker/nrf_common.ld; it defined none of those symbols, so if the INCLUDE
 * search order had ever preferred it the link would have failed outright. It
 * has been removed so the name can only resolve one way. */
INCLUDE "nrf_common.ld"
