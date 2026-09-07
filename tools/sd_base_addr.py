#!/usr/bin/env python3
"""Print the base address of a SoftDevice Intel hex, as 0xXXXXXXXX.

On nRF54L the SoftDevice has no MBR. Instead it exposes a small table of
interrupt handler addresses at its own base address, and the application (here,
the bootloader) forwards the corresponding exceptions to them -- see
nrf_sd_isr.h. That base has to match the blob that is actually flashed, so the
build reads it out of the hex rather than carrying a copy of it in a header.

The base is simply the lowest address the hex writes to.
"""

import sys


def base_address(path):
    lowest = None
    extended = 0

    with open(path) as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line.startswith(":"):
                continue
            try:
                record = bytes.fromhex(line[1:])
            except ValueError:
                raise SystemExit(f"{path}:{lineno}: not a valid Intel hex record")

            count, offset, rectype = record[0], (record[1] << 8) | record[2], record[3]
            data = record[4:4 + count]

            if rectype == 0x00:  # data
                address = extended + offset
                if lowest is None or address < lowest:
                    lowest = address
            elif rectype == 0x04:  # extended linear address
                extended = ((data[0] << 8) | data[1]) << 16
            elif rectype == 0x02:  # extended segment address
                extended = ((data[0] << 8) | data[1]) << 4
            elif rectype == 0x01:  # end of file
                break

    if lowest is None:
        raise SystemExit(f"{path}: no data records, cannot determine a base address")
    return lowest


def main():
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} <softdevice.hex>")
    print("0x%08X" % base_address(sys.argv[1]))


if __name__ == "__main__":
    main()
