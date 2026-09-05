# Changelog

## Unreleased

**Added:** nRF54LM20A support (Cortex-M33 @128 MHz, 2 MB RRAM, 512 KB RAM) and
the first board for it, Seeed XIAO nRF54LM20A (`xiao_nrf54lm20a`). Pin mapping
is taken from the upstream Zephyr board definition; it has not been verified on
hardware.

**Added:** SoftDevice S145 v10.0.1 for nRF54LM20A only, vendored under
`lib/softdevice/s145_nrf54lm20_10.0.1/`. It is the first S145 release with an
nRF54LM20A build. The SoftDevice version is now selected per board via
`SD_VERSION`; nRF54L05/L10/L15 continue to use the vendored v9.0.0 unchanged.

**Changed:** the `nrf_nvic.h` and `ble_l2cap.h` compatibility shims moved from
inside the vendored v9.0.0 SoftDevice tree to `src/`. They are part of this
port rather than of any SoftDevice release — no S145 version ships either
header — and leaving them under `lib/softdevice/` tied them to a single
SoftDevice version, which broke as soon as a second one was added. `src/`
precedes the SoftDevice API directory on the include path, so all boards pick
up the same shims.

**Fixed:** the debug build's `BOOTLOADER_REGION_START` was hardcoded to
`0x16A000` for every variant, which is above the bootloader on nRF54L15 and
outside RRAM entirely on nRF54L10/L05. It now tracks the FLASH origin of each
variant's `*_debug.ld`.

## 0.1.0 — 2026-05-13

Initial release. Adafruit-style DFU bootloader ported to the Nordic
nRF54L series (nRF54L05, nRF54L10, nRF54L15) with SoftDevice S145
v9.0.0.

**Supported boards:** nRF54L15-DK, nRF54L10-DK, nRF54L05-DK,
XIAO nRF54L15, XIAO nRF54L15 Sense.

**Transports:** DFU over serial (UART) and OTA (BLE). No UF2 / USB-CDC
— nRF54L has no USB peripheral.

**Companion packages:**
- PlatformIO platform: [caveman99/platform-nordicnrf54](https://github.com/caveman99/platform-nordicnrf54)
- Arduino core framework: [caveman99/nRF54_Arduino](https://github.com/caveman99/nRF54_Arduino)
