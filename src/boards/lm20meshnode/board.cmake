set(MCU_VARIANT nrf54lm20a)
# The ME54BS13 module carries an nRF54LM20A, so unlike the DK there is no DEVELOP_IN_NRF54LM20B.

# Serial DFU over the native USB port (USB-C J101). The USB stack does not fit in 32 KB, so the
# bootloader takes 64 KB and the application starts at 0x10000; the application has to link with
# nrf54lm20a_s145_v10_bl64k.ld to match.
set(USB_DFU ON)
set(LD_FILE ${CMAKE_CURRENT_LIST_DIR}/../../../linker/nrf54lm20a_bl64k.ld)
add_compile_definitions(CODE_REGION_1_START=0x00010000)
