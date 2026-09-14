# Examples — C++

## Purpose

Small C++ example programs that use the TwoWire (Wire) interface provided by linux-wire.

## Quick safety checklist

- These examples operate on `/dev/i2c-*` devices. Scanning and writes talk to real hardware.
- Do not run write examples against unknown devices — they may change configuration or damage hardware.
- Prefer running as a user in the `i2c` group; use sudo only when necessary.

## Common build/run

Build from the repo root:

```sh
cmake --preset dev
cmake --build --preset dev
```

Presets require CMake 3.20 or newer. If you are on an older CMake, the raw `cmake -S . -B build` flow remains supported as a fallback.

Representative binary names (in `build/dev/`):

- `i2c_scanner_cpp`
- `i2c_scanner_strict_cpp`
- `controller_writer_cpp`
- `controller_reader_cpp`
- `controller_multiplier_cpp`

## Notes

- Use the strict scanner if you need to force a data-phase ACK (it performs a write probe).
- Examples are tiny demos; they are not production-grade I2C tools — use with caution.
