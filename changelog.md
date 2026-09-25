# Changelog

## 0.5.0 — 2026-09-24

Promotes 0.5.0-dev1 and 0.5.0-dev2 to a release. No changes since
0.5.0-dev2.

## 0.5.0-dev2 — 2026-09-24

- The DFU button counts as pressed only if it reads active for 50 ms
  (`BUTTON_SETTLE_MS`, overridable per board). It was sampled once, 100 µs
  after its pull-up was enabled, so on power-on the XIAO nRF54LM20A read a
  still-charging P0.09 as a press and sat in serial DFU until the 6-minute
  inactivity timeout. A released button still returns as soon as the pin
  settles

## 0.5.0-dev1 — 2026-09-20

BLE OTA DFU throughput and link reliability, plus a SoftDevice and nrfx
refresh. Nothing here is verified on hardware yet.

- Slave latency is disabled for the DFU connection with
  `BLE_GAP_OPT_SLAVE_LATENCY_DISABLE`. The transport still asks the central
  for latency 4, inherited from nRF52 where it gave the SoftDevice room for
  slow flash writes; on an inbound firmware download it only let the
  peripheral sleep through connection events. s145 dropped the
  `BLE_GAP_OPT_LOCAL_CONN_LATENCY` upstream used to cancel it
- BLE TX power is set in dBm rather than as a `RADIO_TXPOWER_TXPOWER_*`
  register value, which asked for +9 dBm on nRF54L and was rejected. New
  `BLE_TX_POWER_DBM`, default +8 dBm, overridable per board; the advertising
  set was previously hardcoded to +4 dBm
- nRF54L05, nRF54L10 and nRF54L15 move from SoftDevice s145 9.0.0 to 10.0.1,
  matching nRF54LM20A. 10.0.1 ships one hex per SoC, which also fixes
  nRF54L05 and nRF54L10 being given the nRF54L15 image. The SoftDevice moves
  to 0x5A800 / 0xDA800 / 0x15A800 and the freed 7 KB goes to the application
  data region. **The DFU `--sd-req` FWID changes from 0x3024 to 0x310D**
- XIAO nRF54LM20A advertises as `XIAO_DFU`; a static assertion now enforces
  the 8-character limit, past which the DFU service UUID was silently
  dropped from the advertisement
- nrfx updated to v4.6.0 (MDK 9.1.0)

## 0.4.0 — 2026-09-14

nRF54LM20A support. On the XIAO nRF54LM20A the bootloader starts and answers
on the serial DFU link; application DFU and BLE OTA are not verified yet.

- nRF54LM20A with SoftDevice s145 10.0.1 and the XIAO nRF54LM20A board: same
  layout as the other parts, settings page at 0x1D1000, s145 at 0x1DA800,
  serial DFU on UARTE20
- app_timer runs on TIMER21; TIMER20 belongs to the SoftDevice
- Every NVIC register is cleared before the application starts, not just
  IRQ 0-255
- DFU peer data is placed ahead of its CRC, as the application writes it, so
  bonded OTA DFU no longer falls back to unbonded advertising
- Double-reset marker address comes from the linker script; nRF54L10 and
  nRF54L05 no longer write past the end of RAM
- OTA DFU falls back to serial DFU when no SoftDevice is programmed
- `nrf_nvic.h` and `ble_l2cap.h` shims moved out of the vendored SoftDevice tree

## 0.3.0 — 2026-09-14

- Documentation describes the s145 layout, the button handling per board and
  the CMake build; the Make build, `board.mk` files and the UF2 tooling are
  gone
- Display, NeoPixel and APA102 status LED code removed, no nRF54L board has
  them

## 0.2.0 — 2026-09-13

Ported to the s145 SoftDevice layout on nRF54L, verified on the XIAO nRF54L15.

- Bootloader owns RRAM 0x0 and the vector table (no MBR); application at
  0x8000, settings page just below the SoftDevice, CF2 config at 0x7C00
- SoftDevice interrupt and SVC forwarding to the handler table at the s145 base
- RRAM writes through nrfx_rramc while the SoftDevice is off, `sd_flash_write`
  while it is on; page erase emulated with an all-ones write
- BLE OTA: SoftDevice enabled with a valid LF clock config and a CRACEN seed
- Serial DFU on the board console UARTE20, verified with adafruit-nrfutil
- Bootloader NOINIT peer data and the double-reset marker at the top of RAM
- LF clock stopped at hand-over so the application can pick its own source
- MBR commands report NOT_SUPPORTED: SoftDevice and bootloader self-update are
  not available

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
