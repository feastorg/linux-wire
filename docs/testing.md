# Testing

## Unit Tests (Mocked)

The `tests/` directory contains host-only tests that replace the low-level `lw_*` functions with mocks. They exercise buffer semantics, repeated-start flows, timeout handling, and the strict scanner behavior.

### Running locally

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Presets require CMake 3.20 or newer. If you are on an older CMake, the raw `cmake -S . -B build` flow remains supported as a fallback.

Tests covered:

- Plain `requestFrom` using `lw_read`
- Repeated-start reads via `lw_ioctl_read`
- Internal register helper (`requestFrom(addr, qty, iaddress, isize, sendStop)`)
- Timeout flag propagation when `lw_read` reports `ETIMEDOUT`
- Deferred write flushing when `endTransmission(false)` is not followed by a read
- Deferred write failure handling before follow-on operations
- Strict scanner write failures (logging suppressed in that binary)
- Basic negative-path checks for the C API (`lw_open_bus`, `lw_write`, `lw_ioctl_write`, closed-handle errno handling, etc.)

## Hardware Tests

Mock tests catch logic regressions, but you should still validate on real hardware before tagging releases:

1. **Setup**: Attach an I2C peripheral to `/dev/i2c-1`. You can use the Arduino companion sketch in [examples](./examples.md), which exposes a simple peripheral at `0x40`. Ensure `i2c-dev` is loaded (`sudo modprobe i2c-dev`).
2. **Scanner**: From `build/`, run `sudo ./i2c_scanner`. Confirm your peripheral shows up (the Arduino sketch will report at `0x40`).
3. **Writer**: Run `sudo ./controller_writer` (or pass `--help` to adjust bus/address/register). Watch the Arduino serial log to confirm the byte was received.
4. **Reader / Repeated Start**: Run `sudo ./controller_reader`. It performs `endTransmission(false)` followed by `requestFrom()`, verifying the repeated-start path.
5. **Extended exercise**: `sudo ./controller_multiplier` writes a rolling pattern, then reads it back, providing a simple stress test.
6. **Custom tests**: Integrate `linux_wire` into your application and validate against the target device(s).

## Continuous Integration

`.github/workflows/ci.yml` runs on `ubuntu-22.04` for every push/PR targeting `main`. Steps:

1. Checkout
2. Install `cmake`, `ninja-build`, `gcc`, `g++`
3. `cmake --preset dev`
4. `cmake --build --preset dev`
5. `ctest --preset dev`

CI ensures both the library and tests build cleanly on a baseline similar to Raspberry Pi OS. Always run the hardware checks above before releasing.
