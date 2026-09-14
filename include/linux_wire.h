#ifndef LINUX_WIRE_C_CORE_H
#define LINUX_WIRE_C_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h> /* for ssize_t */

/**
 * Maximum length for I2C device path strings.
 * e.g., "/dev/i2c-1" requires ~12 chars; 64 provides comfortable headroom.
 */
#ifndef LINUX_WIRE_DEVICE_PATH_MAX
#define LINUX_WIRE_DEVICE_PATH_MAX 64
#endif

    /**
     * Simple I2C bus handle for /dev/i2c-* devices.
     * This structure is intentionally minimal for clarity and robustness.
     *
     * Fields:
     *   fd          - File descriptor for /dev/i2c-X (or -1 if closed)
     *   device_path - Path used to open the bus (e.g., "/dev/i2c-1")
     *   timeout_us  - Timeout value in microseconds (0 = no timeout)
     *                 Currently informational only; not enforced by implementation
     *   log_errors  - Non-zero enables perror logging for low-level failures
     */
    typedef struct
    {
        int fd;
        char device_path[LINUX_WIRE_DEVICE_PATH_MAX];
        uint32_t timeout_us;
        int log_errors;
    } lw_i2c_bus;

    /**
     * Open an I2C bus at the specified device path.
     *
     * @param bus Pointer to lw_i2c_bus structure to initialize
     * @param device_path Path to I2C device (e.g., "/dev/i2c-1")
     *                    Must start with "/dev/i2c-" for security
     *
     * @return 0 on success, -1 on error (errno set)
     *
     * Error conditions:
     *   EINVAL - Invalid parameters (NULL pointers, empty path, invalid path format)
     *   ENOENT - Device file doesn't exist
     *   EACCES - Permission denied (user may need to be in 'i2c' group)
     *
     * On success, bus->fd contains a valid file descriptor.
     * On failure, the bus handle is reset to a closed state (`fd == -1`).
     *
     * Example:
     *   lw_i2c_bus bus;
     *   if (lw_open_bus(&bus, "/dev/i2c-1") == 0) {
     *       // success
     *   }
     */
    int lw_open_bus(lw_i2c_bus *bus, const char *device_path);

    /**
     * Close an I2C bus and release its file descriptor.
     * Safe to call multiple times or on an already-closed bus.
     *
     * @param bus Pointer to lw_i2c_bus structure to close
     *
     * After calling this function, bus->fd will be set to -1.
     */
    void lw_close_bus(lw_i2c_bus *bus);

    /**
     * Set the I2C slave address for subsequent read/write operations.
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param addr 7-bit I2C slave address (0x00-0x7F)
     *
     * @return 0 on success, -1 on error (errno set)
     *
     * Error conditions:
     *   EBADF  - Bus not open (fd < 0)
     *   EINVAL - Invalid address
     *   EBUSY  - Device address already in use
     *
     * Note: This uses the I2C_SLAVE ioctl. For 10-bit addressing or
     *       other advanced features, use the ioctl functions directly.
     */
    int lw_set_slave(lw_i2c_bus *bus, uint8_t addr);

    /**
     * @brief Check whether a device acknowledges an address, without
     *        transferring any data.
     *
     * Issues an SMBus Quick Write (I2C_SMBUS ioctl, I2C_SMBUS_QUICK): the
     * address is sent with the write bit and the transaction ends there.
     * This is what `i2cdetect -q` issues. It is the only way to test for a
     * device without reading from it or writing to it - lw_write() and
     * lw_ioctl_write() cannot send a zero-length message.
     *
     * Caveat, from the i2cdetect manual: Quick Write is known to corrupt
     * the Atmel AT24RF08 EEPROM, which is why i2cdetect's default mode
     * uses a one-byte read instead on 0x30-0x37 and 0x50-0x5F. If such a
     * part may be on the bus, skip those ranges or use lw_read() there.
     *
     * The address is first selected with the I2C_SLAVE ioctl. An address
     * owned by a kernel driver is not probed (the kernel refuses with
     * EBUSY, and i2cdetect shows it as "UU"); it is reported as 1.
     *
     * On return the bus's selected address is @p addr, as after
     * lw_set_slave().
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param addr 7-bit I2C address (0x00-0x7F)
     *
     * @return 0 if the address acknowledged;
     *         1 if a kernel driver owns the address (present, not probed);
     *        -1 otherwise (errno set)
     *
     * errno on -1:
     *   EBADF  - Bus not open (fd < 0)
     *   EINVAL - NULL bus or address above 0x7F
     *   ENXIO, EREMOTEIO, EIO, ETIMEDOUT - no acknowledge (device absent)
     *   EOPNOTSUPP - the adapter cannot do SMBus Quick; every address will
     *                report -1 with this errno, so check it once before
     *                sweeping a range
     *
     * Nothing is logged on -1, whatever lw_set_error_logging() is set to:
     * callers sweep whole ranges and a NACK is the normal result.
     */
    int lw_probe(lw_i2c_bus *bus, uint8_t addr);

    /**
     * Write data to the currently-selected I2C slave.
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param data Pointer to data buffer to write
     * @param len Number of bytes to write
     * @param send_stop Currently ignored; each write issues a STOP condition
     *
     * @return Number of bytes written on success, -1 on error (errno set)
     *
     * Error conditions:
     *   EINVAL    - Invalid parameters (NULL pointers, NULL buffer with len > 0)
     *   EBADF     - Bus not open
     *   ENXIO     - No device at selected address (NACK)
     *   ETIMEDOUT - Communication timeout
     *
     * Note: The send_stop parameter is accepted for API compatibility but
     *       currently ignored. Linux userspace write() always completes with
     *       a STOP condition. For repeated-start operations, use lw_ioctl_read()
     *       or lw_ioctl_write() which support combined transactions.
     */
    ssize_t lw_write(lw_i2c_bus *bus,
                     const uint8_t *data,
                     size_t len,
                     int send_stop);

    /**
     * Read data from the currently-selected I2C slave.
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param data Pointer to buffer to receive data
     * @param len Number of bytes to read
     *
     * @return Number of bytes read on success, -1 on error (errno set)
     *
     * Error conditions:
     *   EINVAL   - Invalid parameters (NULL pointers, NULL buffer with len > 0)
     *   EBADF    - Bus not open
     *   ENXIO    - No device at selected address (NACK)
     *   ETIMEDOUT - Communication timeout
     *
     * Note: Make sure to call lw_set_slave() before reading.
     */
    ssize_t lw_read(lw_i2c_bus *bus,
                    uint8_t *data,
                    size_t len);

    /**
     * Perform a combined I2C read with optional internal address write.
     * This implements repeated-start read operations without an intervening STOP.
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param addr 7-bit (or 10-bit) device address
     * @param iaddr Pointer to internal address bytes (register address)
     * @param iaddr_len Number of internal address bytes (0 = no internal address)
     * @param data Pointer to buffer to receive data
     * @param len Number of bytes to read
     * @param flags Bitfield for i2c_msg.flags (e.g., I2C_M_TEN for 10-bit addressing)
     *
     * @return Number of bytes read on success, -1 on error (errno set)
     *
     * Error conditions:
     *   EINVAL   - Invalid parameters or size limits exceeded
     *   EBADF    - Bus not open
     *   ENXIO    - No device at address (NACK)
     *   EOVERFLOW - Size calculation overflow
     *
     * This function is useful for reading device registers:
     *   uint8_t reg_addr = 0x10;
     *   uint8_t data[2];
     *   lw_ioctl_read(&bus, 0x40, &reg_addr, 1, data, 2, 0);
     *
     * Internally, this uses the I2C_RDWR ioctl with two messages:
     *   1. Write message with internal address (if iaddr_len > 0)
     *   2. Read message for data
     *
     * The transaction completes atomically without an intervening STOP.
     */
    ssize_t lw_ioctl_read(lw_i2c_bus *bus,
                          uint16_t addr,
                          const uint8_t *iaddr,
                          size_t iaddr_len,
                          uint8_t *data,
                          size_t len,
                          uint16_t flags);

    /**
     * Perform an I2C write with optional internal address using I2C_RDWR ioctl.
     * This allows writing a register address followed by data in a single message.
     *
     * @param bus Pointer to open lw_i2c_bus
     * @param addr 7-bit (or 10-bit) device address
     * @param iaddr Pointer to internal address bytes (register address)
     * @param iaddr_len Number of internal address bytes (0 = no internal address)
     * @param data Pointer to data buffer to write
     * @param len Number of data bytes to write
     * @param flags Bitfield for i2c_msg.flags (e.g., I2C_M_TEN for 10-bit addressing)
     *
     * @return Number of data bytes written (not including internal address) on success,
     *         -1 on error (errno set)
     *
     * Error conditions:
     *   EINVAL    - Invalid parameters or size limits exceeded
     *   EBADF     - Bus not open
     *   ENOMEM    - Memory allocation failed (for large transfers)
     *   ENXIO     - No device at address (NACK)
     *   EOVERFLOW - Size calculation overflow
     *
     * Example (write 0xFF to register 0x10 of device at 0x40):
     *   uint8_t reg = 0x10;
     *   uint8_t value = 0xFF;
     *   lw_ioctl_write(&bus, 0x40, &reg, 1, &value, 1, 0);
     *
     * Performance note: For transfers <= 256 bytes, this function uses stack
     * allocation. Larger transfers use heap allocation (malloc/free).
     */
    ssize_t lw_ioctl_write(lw_i2c_bus *bus,
                           uint16_t addr,
                           const uint8_t *iaddr,
                           size_t iaddr_len,
                           const uint8_t *data,
                           size_t len,
                           uint16_t flags);

    /**
     * Set timeout value for I2C operations.
     *
     * @param bus Pointer to lw_i2c_bus
     * @param timeout_us Timeout value in microseconds (0 = no timeout)
     *
     * @return 0 on success, -1 on error
     *
     * Note: This is currently informational only and not enforced by the
     *       implementation. Future versions may use select/poll to implement
     *       actual timeout behavior. The value is stored in bus->timeout_us.
     */
    int lw_set_timeout(lw_i2c_bus *bus, uint32_t timeout_us);

    /**
     * Enable or disable perror logging for a given bus.
     *
     * @param bus Pointer to lw_i2c_bus
     * @param enable Non-zero to enable logging, zero to suppress
     *
     * This preference is also re-applied by the C++ wrapper after automatic
     * timeout recovery and fresh `begin()` calls.
     */
    void lw_set_error_logging(lw_i2c_bus *bus, int enable);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_WIRE_C_CORE_H */
