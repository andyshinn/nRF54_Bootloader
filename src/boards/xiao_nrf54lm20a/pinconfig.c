#include "boards.h"

__attribute__((used, section(".bootloaderConfig")))
const uint32_t bootloaderConfig[] =
{
  /* CF2 START */
  0x1e9e10f1, 0x20227a79,                       // magic
  3, 100,                                       // used entries, total entries

  204, 0x200000,                                // FLASH_BYTES = 0x200000 (2 MB)
  205, 0x80000,                                 // RAM_BYTES = 0x80000 (512 KB)
  210, 0x20,                                    // PINS_PORT_SIZE = PA_32

  0, 0, 0, 0, 0, 0, 0, 0
  /* CF2 END */
};
