# Changelog

## 0.2.4 — 2026-09-06

**Fixed:** the bootloader handed the application over with VTOR still pointing
at the bootloader's own vector table, so the first exception the application
took dispatched into the bootloader. A FreeRTOS application hits this
immediately: `vTaskStartScheduler` starts the first task with `svc 0`, which
went to the bootloader's `SVC_Handler` and faulted with MMFSR IACCVIOL.

`bootloader_app_start()` was built on the nRF52 convention where the MBR at
address 0 owns the real vector table and `SD_MBR_COMMAND_IRQ_FORWARD_ADDRESS_SET`
tells it where to forward exceptions -- which is why the nRF52 code never
touched VTOR. nRF54L has no MBR, so that SVC had nothing to reach, and the
fallback it took when the command failed wrote the forward address to the first
word of SRAM: 0x20000000, inside the SoftDevice's RAM region on this part, so
it corrupted SoftDevice state instead. Both are gone; the function now sets
`SCB->VTOR` to the application's vector table, with a DSB/ISB, before jumping.

**Fixed:** only IRQs 0..31 were disabled before the jump. The second
`NVIC->ICER`/`ICPR` write was behind `__NRF_NVIC_ISER_COUNT == 2`, a macro that
comes from the nRF52 SoftDevice headers and is not defined for S145, and
nRF54LM20A numbers its interrupts up to 289 (VREGUSB). Anything left enabled
above IRQ 31 fired into an application that had not finished starting. All 16
Cortex-M33 ICER/ICPR registers are now cleared.

**Fixed:** `is_sd_existed()` looked for `SD_MAGIC_NUMBER` at
`SOFTDEVICE_INFO_STRUCT_OFFSET + 4` (0x2004), which on nRF54L is inside the
application image, not a SoftDevice: S145 is linked at the top of RRAM and the
application always starts at MBR_SIZE. An application that happened to hold the
magic word at 0x2004 would have made the bootloader take `SD_SIZE_GET(MBR_SIZE)`
-- another arbitrary application word -- as `CODE_REGION_1_START`, and so as the
address it validated, erased during DFU, and jumped to. The probe now reports
what is actually true on this family and `CODE_REGION_1_START` is `MBR_SIZE`.

**Fixed:** `INCLUDE "nrf_common.ld"` in every board linker script resolved to
`lib/nrfx/bsp/stable/mdk/nrf_common.ld`, not to the `linker/nrf_common.ld` this
repo shipped — the MDK directory is on the link line as
`-L<nrfx>/bsp/stable/mdk`, and that copy won the INCLUDE search. That was the
only outcome that could ever work: the MDK startup code references
`__data_start`, `__sdata_start`, `__tdata_load_start`, `__fast_start`,
`__tbss_start__` and `__sbss_start__`, and only the MDK script defines them.
Forcing the include to `linker/nrf_common.ld` fails the link with sixteen
undefined references. So the repo's copy was dead code that nothing could use
and that ld might have picked up on a different binutils, in a different
working directory, or with `-L` reordered. It has been deleted so the name can
only resolve one way, and each INCLUDE site now says where it points.

**Fixed:** `.noinit` was placed by ld's orphan handling instead of by the
linker script. `.noinit(NOLOAD) : { } > NOINIT` had no input wildcard, so the
`m_peer_data` / `m_peer_data_crc` block that `src/dfu_ble_svc.c` uses to hand
bond information from the application to the bootloader across a soft reset was
never assigned to it, and the MDK script's own generic `.noinit` rule took the
inputs and stacked them on top of `.data`. Its address therefore moved with the
size of `.data`, which is exactly what a no-init block must not do. The
sections now carry `KEEP(*(.noinit .noinit.*))` and sit at `ORIGIN(RAM)`, ahead
of the MDK rule, so the block has a fixed address: `m_peer_data_crc` at
0x20004800 and `m_peer_data` at 0x20004802 on every board.

The `NOINIT` memory region is gone with it. It declared four bytes at
0x200047FC, which could never have held the ~62 bytes of `.noinit` the
bootloader has; forcing the block into it overflows the region by 58 bytes.
Note the application has to reserve the matching window at its own `ORIGIN(RAM)`
for the handoff to work end to end — the bootloader side is now deterministic,
the contract is not yet declared on the core side.

**Fixed:** `.bootloaderSettings` and `.mbrParamsPage` had no input wildcard
either, so `m_boot_settings` and `m_mbr_params_page` landed on their reserved
pages only because ld merges an orphan into an identically named output
section. `m_mbr_params_page` was in fact not being placed at all. Both now use
`KEEP(*(...))`; `m_boot_settings` is at BOOTLOADER_SETTINGS_ADDRESS and
`m_mbr_params_page` at BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS for every board.

**Fixed:** `src/main.c` reached the double-reset detection word through a
literal `0x200047F8` with nothing tying it to `ORIGIN(DBL_RESET)` in the linker
scripts, so the two could drift apart silently and double-reset DFU entry would
stop working with no build error. The scripts now export `__dbl_reset_mem` and
`main.c` uses that. The address is unchanged on every board.

**Removed:** the `release/` hex files from the source tree. They arrived on
this branch through the v0.2.1 CI release commit and were never refreshed, so a
`-t bootloader` build from the branch — rather than from a release tag — picked
up a v0.2.1 image with no reset vector at address 0 and a stack top of
0x20080000, and flashed a board that could not boot. `AGENTS.md` already says
only release tags carry `release/`; master never had it. CI recreates the
directory on a release event.

## 0.2.3 — 2026-09-06

**Fixed:** nothing occupied address 0, so a board flashed with only this
bootloader locked up at reset and never ran. nRF54L has no MBR: Nordic's model
puts the reset-vector owner at 0x00000000 (in sdk-nrf-bm the application is
slot0 there) and the SoftDevice at the top of RRAM. This port inherited the
nRF52 layout, where an MBR at 0 reads UICR.BOOTLOADERADDR and forwards to the
bootloader — but nRF54L has neither that MBR nor a UICR.BOOTLOADERADDR field,
so the core fetched SP and PC of 0xFFFFFFFF at reset and locked up with
DHCSR.S_LOCKUP set.

`src/boot_stub.S` now provides a reset vector at 0, inside the 4 KB page
already reserved below the application, so it costs no usable flash. It points
VTOR at the bootloader's vector table before jumping — needed because
check_dfu_mode() enables interrupts well before ble_stack_init() sets VTOR, so
a bare SP/PC pair survives reset but dies on the first interrupt. SP and entry
are read from the bootloader's vector table at runtime and its base comes from
the linker as `__bootloader_vectors`, so the stub stays correct across
bootloader DFU updates and across the release and debug layouts.

**Fixed:** dropped the `.uicrBootStartAddress` and `.uicrMbrParamsPageAddress`
sections for nRF54LM20A. They target 0x10001014/0x10001018, which is the nRF52
UICR; on nRF54LM20A the UICR is at 0x00FFD000 and those addresses are not
mapped at all. Emitting them put a segment in the hex at an address the part
does not have, which flashers reject or silently skip.

Verified on a XIAO nRF54LM20A: flashing the bootloader hex plus the SoftDevice,
with no hand-editing, resets into VTOR=0x001D0000, no lockup, UARTE20 live on
P1.11/P1.10, and the bootloader waiting in bootloader_dfu_start() for a serial
DFU host.

Only nRF54LM20A is affected. nRF54L05/L10/L15 keep the nRF52-style layout and
are unchanged; they have the same address-0 gap, which is not addressed here.

## 0.2.2 — 2026-09-06

**Fixed:** the nRF54LM20A RAM region ended at 0x20080000, but the top of that
range is not accessible on silicon — reads and writes above roughly 0x2007FE00
fault. `__StackTop` is `ORIGIN(RAM) + LENGTH(RAM)`, so the initial stack
pointer landed in unusable memory and the very first instruction of
`SystemInit` (`push {r3, lr}`) took a HardFault before `main()` was ever
reached. Confirmed on a XIAO nRF54LM20A over SWD: the core sat in lockup with
`DHCSR.S_LOCKUP` set, and moving the limit to 0x2007FCC0 got execution through
`SystemInit` into `main()` and on into `bootloader_dfu_start()`.

0x2007FCC0 is the end of `app_ram` in Nordic's own partitioning for this SoC
(sdk-nrf-bm `bm_nrf54lm20dk_nrf54lm20a_cpuapp_s145_softdevice.dts`). Note the
flat memory maps disagree with the hardware here: the nrfx MDK linker script,
pyOCD's target definition and TinyUSB's BSP all declare the full 512 KB. Those
get away with it by declaring RAM and RAM1 as two separate regions, which puts
their `__StackTop` at 0x20040000; this script merges them into one region, so
the unusable top mattered.

Only nRF54LM20A is affected — the nRF54L05/L10/L15 scripts are unchanged.

## 0.2.1 — 2026-09-05

**Fixed:** the XIAO nRF54LM20A serial DFU UART was on the wrong pins and the
wrong peripheral, so `nrfutil` over the USB port could never reach it. The
board's USB connector goes to an on-board SAMD11 CMSIS-DAP probe (VID 0x2886,
PID 0x0068) whose CDC bridge lands on P1.11 (TX) / P1.10 (RX); the board was
configured for P1.08/P1.09, the D6/D7 header pins, which the USB port does not
reach.

**Fixed:** the DFU UART peripheral is now selectable per board. It was
hardcoded to `UARTE00` (`SERIAL00`), which is in the fast peripheral domain and
only reaches GPIO port P2 — and on the XIAO nRF54LM20A every P2 pin belongs to
the external SPI flash, so no P1 pin assignment could have worked. Boards now
set `UART_INSTANCE` in `board.cmake` / `board.mk`; `xiao_nrf54lm20a` selects
`UARTE20`. Boards that do not set it keep `SERIAL00`, so no other board
changes behaviour.

**Fixed:** the nRF54LM20A MBR params and bootloader settings pages were at
`0x1FE000` and `0x1FF000`, both past the last usable RRAM address on this part
(`NRF_MEMORY_FLASH_SIZE = 0x1FD000`; the SVD says "2036 KByte RRAM"). Since
`bootloader_app_is_valid()` reads `bank_0` out of the settings page, a page
that is not physically backed would leave the application permanently invalid
and the board stuck in DFU. They now sit at `0x1D8000` and `0x1D9000`, in the
free page-aligned slots between the bootloader config page and the SoftDevice
base. **Consumers must update `bootloader.settings_addr` to `0x1D9000` in
`boards/xiao_nrf54lm20a.json`.**

**Fixed:** `DFU_APP_DATA_RESERVED` for nRF54LM20A was 10 pages (40 KB), which
put the maximum DFU application image at `0x1C5000` while the Arduino core's
`nrf54lm20a_s145_v10.ld` and `upload.maximum_size` both allow `0x1C6000`. An
application in that last 4 KB would have been rejected by the size check. It is
now 9 pages (36 KB) — 28 KB InternalFS plus the 8 KB gap below the bootloader —
so the bootloader, the core's linker script and the platform's board JSON agree
exactly on a `0x1000`–`0x1C7000` application region.

**Fixed:** the `xiao_nrf54lm20a` CF2 config block advertised
`FLASH_BYTES = 0x200000`; the part has `0x1FD000` of usable RRAM.

**Docs:** the README described DFU entry as if every board had two buttons. The
`BUTTON_DFU` / `BUTTON_DFU_OTA` entries are compiled out on boards that do not
define them (all XIAO boards), where double-reset and `GPREGRET` are the only
entry paths. Also corrected `NRF_POWER->GPREGRET` to `GPREGRET[0]`.

## 0.2.0 — 2026-09-05

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
