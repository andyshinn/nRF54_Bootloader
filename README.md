# nRF54L Bootloader

BLE DFU bootloader for Nordic nRF54L series (nRF54L15, nRF54L10, nRF54L05).

Based on the Nordic nRF5 SDK bootloader architecture, ported to nRF54L with SoftDevice S145.

## Supported Boards

| Board | Target | MCU |
|-------|--------|-----|
| nRF54L15-DK | `nrf54l15dk` | nRF54L15 |
| nRF54L10-DK | `nrf54l10dk` | nRF54L10 |
| nRF54L05-DK | `nrf54l05dk` | nRF54L05 |
| XIAO nRF54L15 | `xiao_nrf54l15` | nRF54L15 |
| XIAO nRF54L15 Sense | `xiao_nrf54l15_sense` | nRF54L15 |

## Features

- DFU over Serial (UART) and OTA (BLE)
- Self-upgradable via Serial and OTA (bootloader + SoftDevice)
- Dual bank firmware updates (disabled by default)
- Signed firmware updates (disabled by default)
- Watchdog feed-through during DFU

Note: nRF54L has no USB peripheral, so there is no UF2 or CDC support.

## How to use

The bootloader checks two pins on reset:

- **DFU = LOW**, **FRST = HIGH**: Enter serial DFU mode
- **DFU = LOW**, **FRST = LOW**: Enter OTA DFU mode (for Nordic nRF Connect/Toolbox)
- **DFU = HIGH**, **FRST = HIGH**: Boot application if present, otherwise enter DFU
- **Double Reset** within 500 ms: Enter DFU mode

On the nRF54L15-DK, `DFU` is **Button1** and `FRST` is **Button2**.

The application can also trigger DFU via the `GPREGRET` register:

```c
void reset_to_dfu(void) {
    NRF_POWER->GPREGRET = 0x4e; // 0xA8 for OTA
    NVIC_SystemReset();
}
```

## Building

### Prerequisites

- ARM GCC toolchain (12.x or later)
- CMake 3.17+
- Nordic [nRF Command Line Tools](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Command-Line-Tools) (for flashing)

### Clone

```
git clone --recurse-submodules https://github.com/caveman99/nRF54_Bootloader.git
```

### Build with CMake

```bash
cmake -B _build -DBOARD=nrf54l15dk -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build _build
```

Output: `_build/bootloader.hex`

### Build with Make

```bash
make BOARD=nrf54l15dk all
```

### Flash

Flash bootloader via J-Link:

```bash
make BOARD=nrf54l15dk flash
```

Flash SoftDevice:

```bash
make BOARD=nrf54l15dk flash-sd
```

DFU via serial:

```bash
make BOARD=nrf54l15dk SERIAL=/dev/ttyACM0 flash-dfu
```

If using pyocd:

```bash
make BOARD=nrf54l15dk FLASHER=pyocd flash
```

## Build options

| Option | Default | Description |
|--------|---------|-------------|
| `DUALBANK_FW` | `0` | Split flash into two banks for safe updates |
| `SIGNED_FW` | `0` | Require signed firmware packages |
| `DEFAULT_TO_OTA_DFU` | `0` | Default to OTA DFU instead of serial |

### Signed firmware

Generate a signing key:

```bash
nrfutil keys --gen-key stored_key.pem
nrfutil keys --show-vk code stored_key.pem
```

Build with signing enabled:

```bash
make BOARD=nrf54l15dk SIGNED_FW=1 SIGNED_FW_QX='...' SIGNED_FW_QY='...'
```

Create a signed DFU package:

```bash
nrfutil dfu genpkg --dev-type 0x0054 --sd-req 0x3024 --application app.hex --key-file stored_key.pem app_dfu.zip
```

Upload via serial:

```bash
nrfutil dfu serial -pkg app_dfu.zip -p /dev/ttyACM0 -b 115200
```

## Adding a new board

1. Create `src/boards/<board_name>/` with:
   - `board.h` — pin definitions (LEDs, buttons, UART)
   - `board.cmake` — set `MCU_VARIANT` (nrf54l15, nrf54l10, or nrf54l05)
   - `board.mk` — set `MCU_SUB_VARIANT`
   - `pinconfig.c` — board metadata

2. Build with `cmake -DBOARD=<board_name> ...`

See existing boards in `src/boards/` for reference.

## License

MIT License. See [LICENSE](LICENSE) for details.
