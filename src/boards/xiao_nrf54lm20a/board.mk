MCU_SUB_VARIANT = nrf54lm20a
# S145 v10.0.1 is the first release with an nRF54LM20A build
SD_VERSION = 10.0.1
# DFU UART is on P1.11/P1.10. SERIAL00 only reaches P2 (and every P2 pin on
# this board belongs to the external SPI flash), so drive UARTE20 instead.
UART_INSTANCE = 20
