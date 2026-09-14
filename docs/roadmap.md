# linux-wire Roadmap

**Note:** Items listed here are potential future directions. No commitment is made regarding order, timing, or whether any specific item will be implemented.

**Scope:** linux-wire focuses on I²C **controller mode only**. For the target (kernel "i2c-slave") mode investigation, see [slave-mode-investigation.md](slave-mode-investigation.md).

---

## Performance & Reliability

**Goal:** Production-ready library with comprehensive testing and error handling

**Resources:**

- [i2c-tools](https://git.kernel.org/pub/scm/utils/i2c-tools/i2c-tools.git/) - Reference testing utilities
- [I²C Bus Recovery](https://www.kernel.org/doc/html/latest/i2c/i2c-protocol.html) - Kernel error handling

### Testing & Benchmarks

- Transaction latency at 100kHz, 400kHz, 1MHz
- ioctl (I2C_RDWR) vs read/write performance comparison
- Stress testing: continuous transfers, EMI/noise injection
- Multi-device bus contention scenarios

### Error Handling

- Automatic bus recovery (stuck SDA/SCL)
- Clock stretching timeout handling
- NAK/timeout retry strategies
- Multi-controller arbitration loss recovery

### Documentation

- Performance tuning guide with benchmarks
- Error handling patterns and best practices
- Platform-specific quirks (Raspberry Pi clock stretching)
- Debugging techniques

---

## Advanced Features

**Resources:**

- [I²C Multi-Controller Spec](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [SMBus Specification](http://smbus.org/specs/)

### Multi-Controller Support

- Arbitration loss handling
- Clock stretching configuration (sysfs)
- Bus recovery mechanisms (SCL toggling)
- Raspberry Pi multi-controller documentation

### Extended Addressing & Protocols

- **10-bit addressing:** Examples and platform testing (already supported via I2C_TENBIT)
- **SMBus wrappers:** Convenience helpers for block transfers, PEC, quick commands (libi2c already supports)

---

## Future Considerations

### Distribution

- Debian/Ubuntu packages (.deb)
- Raspberry Pi OS apt repository
- Arch AUR, header-only options

### Language Bindings

- Python (ctypes/cffi), Rust (safe FFI)
- Node.js (N-API), Go (CGo)

### Raspberry Pi Enhancements

- Device tree overlays for I²C configuration ([reference](https://github.com/raspberrypi/linux/tree/rpi-6.1.y/arch/arm/boot/dts/overlays))
- raspi-config integration
- I²C expander examples (MCP23017, PCA9685)
