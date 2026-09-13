#include "bootloader_settings.h"
#include <stdint.h>
#include <dfu_types.h>

__attribute__ ((section(".bootloaderSettings")))
uint8_t m_boot_settings[CODE_PAGE_SIZE];

void bootloader_util_settings_get(const bootloader_settings_t ** pp_bootloader_settings)
{
    *pp_bootloader_settings = (bootloader_settings_t const *) &m_boot_settings[0];
}
