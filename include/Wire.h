#ifndef LINUX_WIRE_CPP_WIRE_H
#define LINUX_WIRE_CPP_WIRE_H

#include <cstddef>
#include <cstdint>

#include "linux_wire.h"

/**
 * Buffer size, mirroring Arduino's default BUFFER_LENGTH (32).
 * You can adjust this at compile time if needed for larger transfers.
 *
 * Note: I2C devices can typically handle 255+ bytes, but Arduino
 * compatibility dictates a 32-byte default. Consider increasing
 * for EEPROM writes, display updates, etc.
 */
#ifndef LINUX_WIRE_BUFFER_LENGTH
#define LINUX_WIRE_BUFFER_LENGTH 32
#endif

/**
 * A minimal, Arduino-compatible TwoWire implementation for Linux.
 *
 * This class provides an Arduino Wire API wrapper around Linux /dev/i2c-* devices.
 * It supports controller-mode I2C communication only.
 *
 * Key differences from Arduino Wire:
 *  - Controller mode only (no peripheral callbacks - Linux userspace I2C limitations)
 *  - No inheritance from Stream / Print classes
 *  - setClock() is a no-op (bus speed configured via device tree/kernel)
 *  - flush() is a no-op (no hardware FIFO in userspace)
 *  - Repeated starts are emulated via I2C_RDWR ioctl calls; they work when using endTransmission(false) + requestFrom().
 *
 * Thread Safety:
 *  - This class is NOT thread-safe
 *  - Do not call methods from multiple threads without external synchronization
 *  - Each TwoWire instance should be used by only one thread
 *
 * Resource Management:
 *  - Manages a file descriptor to /dev/i2c-X
 *  - Copy construction and assignment are deleted (non-copyable resource)
 *  - Move operations could be added in future versions if needed
 */
class TwoWire
{
public:
    static constexpr std::size_t INTERNAL_ADDRESS_MAX = 4;

    TwoWire();
    ~TwoWire();

    /* Delete copy operations - this class manages OS resources (file descriptor) */
    TwoWire(const TwoWire &) = delete;
    TwoWire &operator=(const TwoWire &) = delete;

    /**
     * Initialize I2C communication on a specific device path.
     *
     * @param device Path to I2C device (default: "/dev/i2c-1")
     *               Must be in format "/dev/i2c-N" for security
     *
     * Example:
     *   Wire.begin();              // Uses /dev/i2c-1
     *   Wire.begin("/dev/i2c-0");  // Uses /dev/i2c-0
     *
     * Note: If already open, this will close and reopen the bus.
     *       Stored timeout and error-logging preferences are re-applied
     *       after every successful open.
     */
    void begin(const char *device = "/dev/i2c-1");

    /**
     * Arduino-style overloads for peripheral (target) mode compatibility.
     * On Linux these are no-ops (target mode not supported in userspace).
     *
     * @param address Peripheral address (ignored)
     *
     * Note: Linux I2C target support requires kernel-mode drivers.
     */
    void begin(uint8_t address);
    void begin(int address);

    /**
     * Close the underlying I2C bus and release resources.
     * Safe to call multiple times.
     */
    void end();

    /**
     * Set I2C bus clock speed.
     *
     * @param frequency Desired frequency in Hz (ignored)
     *
     * Note: This is a no-op on Linux. Bus speed must be configured via:
     *       - Device tree overlays
     *       - Kernel module parameters
     *       - sysfs interfaces (/sys/class/i2c-adapter/...)
     *
     * This method exists for Arduino API compatibility only.
     */
    void setClock(uint32_t frequency);

    /**
     * Configure timeout behavior for I2C operations.
     *
     * @param timeout_us Timeout in microseconds (default: 25ms)
     * @param reset_with_timeout If true, bus will be reset on timeout (default: false)
     *
     * Note: Timeout enforcement is currently informational only.
     *       Future versions may implement actual timeout via select/poll.
     *       The configured timeout hint is preserved across `begin()` and
     *       timeout-triggered reopen operations.
     */
    void setWireTimeout(uint32_t timeout_us = 25000, bool reset_with_timeout = false);

    /**
     * Check if a timeout has occurred.
     *
     * @return true if timeout flag is set, false otherwise
     */
    bool getWireTimeoutFlag(void) const;

    /**
     * Clear the timeout flag.
     */
    void clearWireTimeoutFlag(void);

    /**
     * Enable or disable error messages printed by the low-level I2C helpers.
     * Useful when deliberately probing addresses that will NACK (e.g., strict scanner).
     *
     * The chosen setting is preserved across `begin()` and timeout-triggered
     * reopen operations.
     */
    void setErrorLogging(bool enable);

    /**
     * Begin a controller transmission to the specified I2C address.
     *
     * @param address 7-bit I2C target address
     *
     * After calling this, use write() to queue data, then call
     * endTransmission() to actually send it.
     *
     * Example:
     *   Wire.beginTransmission(0x40);
     *   Wire.write(0x00);  // register address
     *   Wire.write(0xFF);  // data
     *   Wire.endTransmission();
     */
    void beginTransmission(uint8_t address);
    void beginTransmission(int address);

    /**
     * End a transmission and send queued data to the I2C bus.
     *
     * @param sendStop If 0, don't send STOP (prepare for repeated start)
     *                 If non-zero, send STOP (default behavior)
     *
     * @return Error code:
     *         0 = success
     *         1 = data too long for buffer
     *         2 = NACK on address (only from an empty transmission, which
     *             probes the address with lw_probe() instead of writing)
     *         4 = other error (bus not open, NACK on data, timeout, etc.)
     *
     * An empty transmission with sendStop != 0 is the Arduino "is anyone
     * there?" idiom. It issues an SMBus Quick Write, which can corrupt an
     * Atmel AT24RF08 EEPROM (see lw_probe()); avoid probing 0x30-0x37 and
     * 0x50-0x5F if one may be present. A driver-owned address counts as 0.
     *
     * Note: sendStop=0 enables repeated-start emulation for the next
     *       requestFrom() call, avoiding an intervening STOP condition.
     *       The actual STOP is still issued by the kernel driver; this
     *       just controls whether to use combined ioctl transactions.
     *       If an older deferred write must be auto-flushed first and that
     *       flush fails, this method returns 4 and does not send the new
     *       transmission.
     */
    uint8_t endTransmission(uint8_t sendStop);
    uint8_t endTransmission(void);

    /**
     * Request data from an I2C peripheral.
     *
     * @param address 7-bit I2C target address
     * @param quantity Number of bytes to request (max: LINUX_WIRE_BUFFER_LENGTH)
     * @param iaddress Internal register address (for register reads)
     * @param isize Size of internal address in bytes (1-4)
     * @param sendStop Currently ignored; Linux userspace always sends STOP
     *
     * @return Number of bytes actually read (0 on error)
     *
     * After successful call, use available(), read(), and peek() to
     * access the received data.
     *
     * If a deferred write from `endTransmission(false)` must be auto-flushed
     * before starting this read and that flush fails, this method returns 0
     * and does not start the read.
     *
     * Examples:
     *   // Simple read
     *   Wire.requestFrom(0x40, 2);
     *
     *   // Register read (write register address, then read data)
     *   Wire.requestFrom(0x40, 2, 0x10, 1);  // Read 2 bytes from register 0x10
     *
     *   // Repeated-start read
     *   Wire.beginTransmission(0x40);
     *   Wire.write(0x00);  // register
     *   Wire.endTransmission(0);  // don't send STOP
     *   Wire.requestFrom(0x40, 1);  // combined transaction
     */
    uint8_t requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop);
    uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop);
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    uint8_t requestFrom(int address, int quantity);
    uint8_t requestFrom(int address, int quantity, int sendStop);

    /**
     * Write data to the TX buffer.
     * Must be called between beginTransmission() and endTransmission().
     *
     * @param data Byte(s) to write
     * @param quantity Number of bytes (for array overload)
     * @param str Null-terminated string (for string overload)
     *
     * @return Number of bytes written (0 if buffer full or not transmitting)
     *
     * Note: Data is queued in a buffer and sent when endTransmission() is called.
     */
    size_t write(uint8_t data);
    size_t write(const uint8_t *data, size_t quantity);
    size_t write(const char *str);

    /**
     * Get number of bytes available for reading from RX buffer.
     * Must be called after requestFrom().
     *
     * @return Number of bytes available
     */
    int available(void) const;

    /**
     * Read one byte from RX buffer.
     *
     * @return Byte value (0-255), or -1 if no data available
     */
    int read(void);

    /**
     * Peek at the next byte in RX buffer without consuming it.
     *
     * @return Byte value (0-255), or -1 if no data available
     */
    int peek(void);

    /**
     * Flush output buffer.
     *
     * Note: This is a no-op on Linux (no hardware FIFO in userspace).
     *       Exists for Arduino API compatibility only.
     */
    void flush(void);

private:
    lw_i2c_bus bus_;
    bool bus_open_;
    bool errorLoggingEnabled_;

    char devicePath_[LINUX_WIRE_DEVICE_PATH_MAX];
    uint8_t txAddress_;
    bool transmitting_;
    bool hasPendingTxForRead_;

    uint8_t txBuffer_[LINUX_WIRE_BUFFER_LENGTH];
    std::size_t txBufferIndex_;
    std::size_t txBufferLength_;

    uint8_t rxBuffer_[LINUX_WIRE_BUFFER_LENGTH];
    std::size_t rxBufferIndex_;
    std::size_t rxBufferLength_;

    uint32_t wireTimeoutUs_;
    bool wireTimeoutFlag_;
    bool wireResetOnTimeout_;
    bool inTimeoutHandler_;

    void resetTxBuffer();
    void resetRxBuffer();
    void applyBusConfiguration();

    uint8_t requestFrom(uint8_t address,
                        uint8_t quantity,
                        const uint8_t *internalAddress,
                        std::size_t internalAddressLength,
                        uint8_t sendStop,
                        bool consumePendingTx);

    void handleTimeoutFromErrno();
    bool reopenBus(const char *device);
    bool flushPendingRepeatedStart();
};

/**
 * Global Wire instance, matching Arduino API.
 *
 * Usage:
 *   Wire.begin();
 *   Wire.beginTransmission(0x40);
 *   Wire.write(data);
 *   Wire.endTransmission();
 *
 * THREAD SAFETY WARNING:
 *   This global instance is NOT thread-safe. Do not access from multiple
 *   threads without external synchronization (e.g., mutex).
 *   For multi-threaded applications, create separate TwoWire instances
 *   per thread, each with its own device path.
 *
 * Note: Only one global instance is provided. For multiple I2C buses,
 *       create separate TwoWire instances and call begin() with different
 *       device paths (e.g., "/dev/i2c-0", "/dev/i2c-1").
 */
extern TwoWire Wire;

#endif /* LINUX_WIRE_CPP_WIRE_H */
