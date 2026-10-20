# linux-wire

[![CI](https://github.com/feastorg/linux-wire/actions/workflows/ci.yml/badge.svg)](https://github.com/feastorg/linux-wire/actions/workflows/ci.yml)

linux-wire is a minimal Linux-native I2C abstraction that mirrors Arduino's `Wire` API. It ships a tiny C shim over `/dev/i2c-*` plus a `TwoWire` C++ class so Arduino-style sketches can run on Raspberry Pi-class systems without heavyweight dependencies.

[![License: MPL-2.0](https://img.shields.io/badge/License-MPL--2.0-blue.svg)](./LICENSE)
![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20C-00599C)
![CMake](https://img.shields.io/badge/build-CMake-064F8C)

## Quick Start

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Other canonical builds:

```sh
# optimized full build
cmake --preset release
cmake --build --preset release
ctest --preset release

# lean consumer-style build
cmake --preset minimal
cmake --build --preset minimal
```

Use `cmake --install build/dev` to install the default preset build, or pass `--prefix` as needed. Downstream CMake projects can then `find_package(linux_wire CONFIG REQUIRED)` and link against `linux_wire::linux_wire`.

Presets require CMake 3.20 or newer. If you are on an older CMake, the raw `cmake -S . -B build` flow remains supported as a fallback.

Example binaries for the default contributor build live under `build/dev/` after compiling.

## Documentation

- [Docs index](./docs/index.md) – architecture overview, API examples, CI info
- [API reference](./docs/api.md) – detailed class and method documentation
- [Testing guide](./docs/testing.md) – running mock-based unit tests and suggested hardware validation steps
- [Arduino companion](./docs/examples.md) – ready-made Nano sketch for loopback testing

## License

MPL-2.0. See [LICENSE](./LICENSE).
