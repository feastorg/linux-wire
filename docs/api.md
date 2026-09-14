# API Reference

This document summarizes the public interfaces exposed by linux-wire. For usage examples and build instructions, see [docs/index.md](./index.md).

---

## C API (`linux_wire.h`)

```c
typedef struct
{
    int fd;
    char device_path[LINUX_WIRE_DEVICE_PATH_MAX];
    uint32_t timeout_us;
    int log_errors;
} lw_i2c_bus;
```

### Bus Management

| Function                                                    | Description                                                                                        |
| ----------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `int lw_open_bus(lw_i2c_bus *bus, const char *path);`       | Opens `/dev/i2c-X` and populates the handle. Returns `0` on success, `-1` on error (sets `errno`) and resets the handle to a closed state on failure. |
| `void lw_close_bus(lw_i2c_bus *bus);`                       | Closes the file descriptor if open. Safe to call multiple times.                                   |
| `int lw_set_slave(lw_i2c_bus *bus, uint8_t addr);`          | Issues `I2C_SLAVE` ioctl to select the target address. Rejects values above `0x7F` with `EINVAL`. |
| `int lw_probe(lw_i2c_bus *bus, uint8_t addr);`              | SMBus Quick Write (what `i2cdetect -q` issues): address + write bit, no data. Returns `0` if the address acknowledged, `1` if a kernel driver owns it (present, not probed), `-1` otherwise (`errno` set; `EOPNOTSUPP` = adapter cannot do Quick). Never logs. The only zero-data probe; `lw_write`/`lw_ioctl_write` reject empty messages. Quick Write can corrupt an Atmel AT24RF08 — avoid on 0x30-0x37 / 0x50-0x5F if one may be present. |
| `int lw_set_timeout(lw_i2c_bus *bus, uint32_t timeout_us);` | Stores a timeout hint (currently informational).                                                   |

### Simple Read/Write

| Function                                                                             | Description                                                                                                      |
| ------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------- |
| `ssize_t lw_write(lw_i2c_bus *bus, const uint8_t *data, size_t len, int send_stop);` | Writes `len` bytes via `write(2)`. `send_stop` is accepted for parity with the C++ API but always issues a STOP. |
| `ssize_t lw_read(lw_i2c_bus *bus, uint8_t *data, size_t len);`                       | Reads up to `len` bytes via `read(2)`.                                                                           |
| `void lw_set_error_logging(lw_i2c_bus *bus, int enable);`                            | Enable (`enable != 0`) or suppress (`enable == 0`) `perror` logging for that bus handle.                         |

Both functions return the number of bytes processed or `-1` on error (`errno` set). Closed handles report `EBADF`; malformed arguments report `EINVAL`.

### Combined Transactions

| Function                                                                                                                                           | Description                                                                                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `ssize_t lw_ioctl_read(lw_i2c_bus *bus, uint16_t addr, const uint8_t *iaddr, size_t iaddr_len, uint8_t *data, size_t len, uint16_t flags);`        | Issues an `I2C_RDWR` ioctl with an optional internal register write followed by a read (repeated-start behavior). |
| `ssize_t lw_ioctl_write(lw_i2c_bus *bus, uint16_t addr, const uint8_t *iaddr, size_t iaddr_len, const uint8_t *data, size_t len, uint16_t flags);` | Builds a single write message combining an optional internal address and payload.                                 |

The helpers validate inputs (non-null buffers, length ≤ 4096, etc.) before calling into the kernel. Closed handles report `EBADF`; malformed arguments report `EINVAL`.

---

## C++ API (`Wire.h`)

`TwoWire` mirrors the Arduino Wire API for master-mode use. A global `TwoWire Wire;` instance is provided, but you can instantiate additional objects if desired.

### Core Methods

| Method                                                                               | Description                                                                                                                                        |
| ------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `void begin(const char *device = "/dev/i2c-1");`                                     | Opens the specified I²C device. Calling `begin()` again reopens the bus and re-applies stored timeout/logging preferences.                           |
| `void begin(uint8_t); void begin(int);`                                              | Provided for Arduino compatibility; they’re no-ops in Linux master mode.                                                                           |
| `void end();`                                                                        | Closes the bus and clears buffers.                                                                                                                 |
| `void setClock(uint32_t frequency);`                                                 | Currently a no-op (bus speed is controlled by the kernel).                                                                                         |
| `void setWireTimeout(uint32_t timeout_us = 25000, bool reset_with_timeout = false);` | Stores a timeout threshold; when the underlying I/O reports `ETIMEDOUT`, `getWireTimeoutFlag()` becomes true and (optionally) the bus is reopened with the same timeout hint still applied. |
| `bool getWireTimeoutFlag() const;` / `void clearWireTimeoutFlag();`                  | Query/reset the timeout flag.                                                                                                                      |
| `void setErrorLogging(bool enable);`                                                 | Toggle low-level `perror` logging (handy when probing addresses that are expected to NACK). The preference survives reopen operations.              |

### Master Transmit

| Method                                                                                                              | Description                                                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `void beginTransmission(uint8_t address);`                                                                          | Starts buffering data for the given device.                                                                                                                                                                           |
| `void beginTransmission(int address);`                                                                              | Overload that forwards to the `uint8_t` version.                                                                                                                                                                      |
| `uint8_t endTransmission(uint8_t sendStop = 1);`                                                                    | Writes the buffered bytes. Return codes match Arduino: `0` success, `1` buffer overflow, `4` other error. Passing `0` for `sendStop` defers the actual write until the next `requestFrom` (repeated-start semantics). If an older deferred write must be auto-flushed first and that flush fails, this call returns `4`. |
| `size_t write(uint8_t data);` / `size_t write(const uint8_t *data, size_t len);` / `size_t write(const char *str);` | Append data to the TX buffer (up to 32 bytes).                                                                                                                                                                        |

### Master Receive

| Method                                                                                                              | Description                                                                                                                                        |
| ------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop = 1);`                                     | Reads up to `quantity` bytes. If prior `endTransmission(false)` buffered data for the same address, it performs a combined `I2C_RDWR` transaction. If a deferred write must be auto-flushed first and that flush fails, the read is aborted and `0` is returned. |
| `uint8_t requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop);`       | Arduino-style register helper; `isize` is clamped to 4.                                                                                            |
| `uint8_t requestFrom(int address, int quantity);` / `uint8_t requestFrom(int address, int quantity, int sendStop);` | Compatibility overloads.                                                                                                                           |
| `int available() const; int read(); int peek(); void flush();`                                                      | Buffer inspection helpers matching Arduino semantics.                                                                                              |

### Repeated-start semantics

- `endTransmission(false)` leaves the TX buffer intact for the next `requestFrom` to the same address.
- If you later target a different address or call `beginTransmission` without a read in between, linux-wire automatically flushes the pending data with a STOP.
- If that automatic flush fails, the follow-on operation fails instead of silently proceeding.

### Timeout behavior

- When `lw_read`/`lw_ioctl_read` reports `ETIMEDOUT`, `wireTimeoutFlag_` is set.
- If `reset_with_timeout` was true when `setWireTimeout` was called, the bus descriptor is closed and reopened automatically.
- Stored timeout and error-logging preferences are re-applied after that reopen.

---

## Examples

See the `examples/` directory for concrete flows:

- `i2c_scanner`: iterates over addresses and uses `endTransmission()` to probe each one.
- `master_writer`: simple register write.
- `master_reader`: demonstrates `endTransmission(false)` + `requestFrom` repeated-start read.

For mock-based tests of buffer management, deferred-write failure handling, and timeout logic, inspect `tests/test_wire.cpp`.
