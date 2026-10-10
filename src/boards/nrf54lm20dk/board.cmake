set(MCU_VARIANT nrf54lm20a)
# The DK carries an nRF54LM20B: the build targets the A, but the errata checks in the MDK only
# match the B's FICR (0x33 at 0x00FFC340) with this, so the B's workarounds get applied.
add_compile_definitions(DEVELOP_IN_NRF54LM20B)

# Serial DFU over the native USB port (J3) instead of the J-Link's UART. The USB stack does not
# fit in 32 KB, so the bootloader takes 64 KB and the application starts at 0x10000; the
# application has to link with nrf54lm20a_s145_v10_bl64k.ld to match.
set(USB_DFU ON)
set(LD_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../linker/nrf54lm20a_bl64k.ld)
add_compile_definitions(CODE_REGION_1_START=0x00010000)
