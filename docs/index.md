# linux-wire Documentation

## Overview

`linux-wire` is a minimal Linux-native implementation of Arduino's `Wire` API. The goal is to provide a tiny C backend over `/dev/i2c-*` and a drop-in `TwoWire` C++ wrapper so Raspberry Pi-class systems can reuse Arduino-style sketches without pulling in heavy dependencies. The library is master-mode only and mirrors Arduino buffer semantics (`BUFFER_LENGTH = 32`).

Key design points:

- C shim (`linux_wire.h` / `src/linux_wire.c`) handles `open`/`ioctl` interactions with `i2c-dev`.
- C++ `TwoWire` class (`Wire.h` / `Wire.cpp`) forwards user-facing Arduino methods to the C shim.
- Repeated-start reads are implemented via a combined `I2C_RDWR` ioctl so register reads behave like the AVR Wire reference.
- Internal TX/RX buffers stay fixed-size for deterministic behavior.

For a quick summary and project tree, see [README.md](../README.md).

## Building

Run the canonical preset-based flows from the project root:

```sh
# default contributor build
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# optimized full build
cmake --preset release
cmake --build --preset release
ctest --preset release

# lean consumer-style build
cmake --preset minimal
cmake --build --preset minimal

# optionally install the dev build
cmake --install build/dev --prefix /desired/install/path

# clean one configured build tree
cmake --build --preset dev --target clean
```

Notes:

- The project ships a `linux_wire` static library plus C and C++ example executables when examples are enabled.
- On Linux, no additional link libraries are typically needed beyond `pthread`/`rt` provided by the toolchain.
- `cmake --install build/<preset> --prefix <dest>` installs headers into `<dest>/include` and exports a `linux_wire::linux_wire` CMake target so downstream projects can simply `find_package(linux_wire CONFIG REQUIRED)`.
- Presets require CMake 3.20 or newer. If you are on an older CMake, the raw `cmake -S . -B build` flow remains supported as a fallback.

## Running Examples

After building with `dev` or `release`, the example binaries live under that preset's build tree:

```sh
# Scan for devices on /dev/i2c-1:
sudo ./build/dev/i2c_scanner_cpp /dev/i2c-1
# Read 2 bytes from register 0x00 of device at 0x40:
sudo ./build/dev/master_reader_cpp /dev/i2c-1 0x40 0x00 2
# Write 3 bytes (0x01, 0x02, 0x03) to register 0x00 of device at 0x40:
sudo ./build/dev/master_writer_cpp /dev/i2c-1 0x40 0x00 0x01 0x02 0x03
```

## Usage Guide

### C API

```c
#include "linux_wire.h"

lw_i2c_bus bus;
lw_open_bus(&bus, "/dev/i2c-1");
lw_set_slave(&bus, 0x40);
uint8_t value = 0xAB;
lw_write(&bus, &value, 1, 1);
lw_close_bus(&bus);
```

The C API exposes `lw_open_bus`, `lw_set_slave`, `lw_probe`, `lw_write`, `lw_read`, `lw_ioctl_read`, and `lw_ioctl_write`. These match the semantics of `_reference/libi2c`, but the surface area stays intentionally small for clarity. All functions validate buffers and return `-1` with `errno` set on failure.

### C++ `TwoWire`

```cpp
#include "Wire.h"

int main() {
    Wire.begin("/dev/i2c-1");
    Wire.beginTransmission(0x40);
    Wire.write(0x00); // register
    Wire.endTransmission(false); // hold the bus
    Wire.requestFrom(0x40, static_cast<uint8_t>(2)); // combined ioctl read
    while (Wire.available()) {
        int byte = Wire.read();
        // ...
    }
    Wire.end();
}
```

Important Arduino-parity notes:

- `requestFrom(address, quantity, iaddress, isize, sendStop)` exists and clamps `isize` to 4 bytes, mirroring the upstream Wire overload.
- Calling `endTransmission(false)` leaves the TX buffer intact for the next `requestFrom` to the same address. If no read follows, the buffer is flushed with a STOP before the next operation; if that automatic flush fails, the follow-on operation fails instead of silently continuing.
- `setWireTimeout(timeout_us, reset_on_timeout)` sets a flag when Linux reports `ETIMEDOUT`. If `reset_on_timeout` is true, the bus handle is closed and reopened automatically (matching the AVR `twi` reset behavior), and stored timeout/logging preferences are re-applied.

## Testing

Software-only tests live in `tests/`. They use a mock `lw_*` backend to exercise buffer management, repeated-start logic, deferred-write failure handling, and timeout handling without real hardware:

```sh
ctest --preset dev
```

For hardware validation (recommended before release):

1. Connect a known I²C device to `/dev/i2c-1` (Raspberry Pi default).
2. Run `./build/i2c_scanner` to ensure the device shows up.
3. Adapt `examples/master_reader` or `examples/master_writer` with the target address/registers and confirm reads/writes succeed.

See [docs/testing.md](./testing.md) for detailed steps.

## CI

GitHub Actions runs on `ubuntu-22.04` (closest to Raspberry Pi OS Bookworm). The workflow installs CMake/Ninja/g++ and executes the canonical `dev` preset build + test flow automatically on every push/PR targeting `main`.

## License

`linux-wire` is GPL-3.0-or-later. See [LICENSE](../LICENSE) for details.
