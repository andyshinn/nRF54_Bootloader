/* NRF54LM20A bootloader with the s145 SoftDevice.
 *
 * s145 has no MBR, so the bootloader owns RRAM 0x0 and the reset vector. Layout:
 *   0x00000000  bootloader (31 KB code + 1 KB CF2 config)
 *   0x00008000  application (own vector table, started by bootloader_app_start)
 *   0x001d1000  bootloader settings page (LittleFS ends one page below)
 *   0x001da800  SoftDevice s145, up to the end of RRAM
 * The SoftDevice keeps RAM 0x20000000-0x20004800. Only the first RAM block is used, so the
 * last 128 bytes below 0x20040000 hold the double-reset marker and the BLE peer data
 * handed over from the application, as on NRF54L15. */
SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x7C00
  BOOTLOADER_CONFIG (r) : ORIGIN = 0x00007C00, LENGTH = 0x400
  BOOTLOADER_SETTINGS (rw) : ORIGIN = 0x001d1000, LENGTH = 0x1000

  RAM (rwx) : ORIGIN = 0x20004800, LENGTH = 0x3b780
  NOINIT (rwx) : ORIGIN = 0x2003ff80, LENGTH = 0x78
  DBL_RESET (rwx) : ORIGIN = 0x2003fff8, LENGTH = 0x04
}

SECTIONS
{
  __dbl_reset_mem = ORIGIN(DBL_RESET);

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

  .dbl_reset(NOLOAD) :
  {
  } > DBL_RESET

  .noinit(NOLOAD) :
  {
    /* The application hands over the peer data followed by its CRC; keep that order */
    KEEP(*(.noinit.peer_data))
    __peer_data_crc = .;
    KEEP(*(.noinit.peer_data_crc))
    *(.noinit*)
  } > NOINIT
  ASSERT(__peer_data_crc == ORIGIN(NOINIT) + 60, "DFU peer data CRC must follow the 60-byte peer data")
}

INCLUDE "nrf_common.ld"
