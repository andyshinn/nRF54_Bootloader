#include "boards.h"
#include "dfu_types.h"

// Virtual disk size: just under 32MB
#define CFG_UF2_NUM_BLOCKS            0x10109

// Board-specific ID for board-specific Application
#if defined(USB_DESC_VID) && defined(USB_DESC_UF2_PID) && USB_DESC_VID && USB_DESC_UF2_PID
    #define CFG_UF2_BOARD_APP_ID      ((USB_DESC_VID << 16) | USB_DESC_UF2_PID)
#endif

// Family ID and size for updating generic Application.
// microsoft/uf2 has no nRF54 family (utils/uf2families.json as of 2026-02), so this one was picked
// at random, as the UF2 spec asks, for the nRF54LM20 (A and B).
#if defined(NRF54LM20A_XXAA)
  #define CFG_UF2_FAMILY_APP_ID       0x9EEA6BF9
  #define CFG_UF2_FLASH_SIZE          (2036*1024) // RRAM
#else
  #error "No UF2 family ID for this MCU"
#endif

// Application Address Space: from the application start up to its data (InternalFS). There is
// no MBR on nRF54L: the bootloader sits below and the settings page and the SoftDevice above,
// and a UF2 file never writes any of them.
#define USER_FLASH_START              CODE_REGION_1_START
#define USER_FLASH_END                (DFU_APP_REGION_END - (DFU_APP_DATA_RESERVED))
