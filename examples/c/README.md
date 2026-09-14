# Examples — C

## Purpose

This folder contains small C example programs that exercise the linux-wire C API (examples/cpp are the C++ equivalents).

## Safety first — read before running

- These examples talk to /dev/i2c-\* devices. That is literal hardware (buses and devices).
- Scanning and writing may change device state and can harm peripherals. Do NOT run write examples against hardware you don't own/understand.

## Permissions and access

- Prefer running as an unprivileged user that belongs to the `i2c` group. If you need elevated permissions, use sudo.
- Example device path: `/dev/i2c-1`. Adjust to your platform (e.g. `/dev/i2c-0` or other bus numbers).

## How to build and run

1. From the project root:

   ```sh
   cmake --preset dev
   cmake --build --preset dev
   ```

   Presets require CMake 3.20 or newer. If you are on an older CMake, the raw `cmake -S . -B build` flow remains supported as a fallback.

2. Examples are built into `build/dev/` with names like:
   - `i2c_scanner_c` — address-only probe (`lw_probe`, SMBus Quick Write)
   - `i2c_scanner_strict_c` — stricter probe (forces a data write)
   - `master_writer_c` — write a register
   - `master_reader_c` — read a register (repeated-start)
   - `master_multiplier_c` — demo request/response

## Quick safety checklist

- Confirm the correct bus file exists (e.g., `ls /dev/i2c-*`).
- Prefer the strict scanner if you need to avoid false positives, but be aware it performs writes.
- Never run write examples unless you know the target device address and register behavior.

## Questions/Problems

If you hit permission or device-not-found errors, check group membership, device path, and that the I2C kernel driver is loaded.
