#include "Wire.h"
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cassert>

TwoWire::TwoWire()
    : bus_open_(false),
      errorLoggingEnabled_(true),
      devicePath_{0},
      txAddress_(0),
      transmitting_(false),
      hasPendingTxForRead_(false),
      txBufferIndex_(0),
      txBufferLength_(0),
      rxBufferIndex_(0),
      rxBufferLength_(0),
      wireTimeoutUs_(0),
      wireTimeoutFlag_(false),
      wireResetOnTimeout_(false),
      inTimeoutHandler_(false)
{
    bus_.fd = -1;
    bus_.device_path[0] = '\0';
    bus_.timeout_us = 0;
    bus_.log_errors = 1;
}

TwoWire::~TwoWire()
{
    end();
}

void TwoWire::begin(const char *device)
{
    if (!device || device[0] == '\0')
    {
        return;
    }

    /* Clean up existing connection fully before attempting new one */
    if (bus_open_)
    {
        bool flushed = flushPendingRepeatedStart();
        resetTxBuffer();
        resetRxBuffer();
        lw_close_bus(&bus_);
        bus_open_ = false;
        if (!flushed)
        {
            return;
        }
    }

    /* Copy device path */
    size_t len = std::strlen(device);
    if (len >= sizeof(devicePath_))
    {
        len = sizeof(devicePath_) - 1;
    }
    std::memcpy(devicePath_, device, len);
    devicePath_[len] = '\0';

    /* Attempt to open - if this fails, state is already clean */
    if (lw_open_bus(&bus_, devicePath_) == 0)
    {
        bus_open_ = true;
        applyBusConfiguration();
        resetTxBuffer();
        resetRxBuffer();
    }
}

void TwoWire::begin(uint8_t /*address*/)
{
    /* Slave mode is not supported on Linux user-space.
       Linux I2C slave support requires kernel-mode drivers.
       This method exists for Arduino API compatibility only. */
}

void TwoWire::begin(int address)
{
    begin(static_cast<uint8_t>(address));
}

void TwoWire::end()
{
    if (bus_open_)
    {
        flushPendingRepeatedStart();
        lw_close_bus(&bus_);
    }
    bus_open_ = false;
    resetTxBuffer();
    resetRxBuffer();
}

void TwoWire::setClock(uint32_t /*frequency*/)
{
    /* Not supported on Linux userspace I2C.
       Bus frequency must be configured via:
       - Device tree overlays
       - Kernel module parameters
       - sysfs interfaces
       This method exists for Arduino API compatibility only. */
}

void TwoWire::setWireTimeout(uint32_t timeout_us, bool reset_with_timeout)
{
    wireTimeoutUs_ = timeout_us;
    wireResetOnTimeout_ = reset_with_timeout;
    wireTimeoutFlag_ = false;

    /* Store timeout in C core; currently informational.
       Future implementation could use select/poll for enforcement. */
    lw_set_timeout(&bus_, timeout_us);
}

bool TwoWire::getWireTimeoutFlag(void) const
{
    return wireTimeoutFlag_;
}

void TwoWire::clearWireTimeoutFlag(void)
{
    wireTimeoutFlag_ = false;
}

void TwoWire::setErrorLogging(bool enable)
{
    errorLoggingEnabled_ = enable;
    lw_set_error_logging(&bus_, enable ? 1 : 0);
}

void TwoWire::beginTransmission(uint8_t address)
{
    if (!flushPendingRepeatedStart())
    {
        transmitting_ = false;
        return;
    }

    transmitting_ = true;
    txAddress_ = address;
    resetTxBuffer();
}

void TwoWire::beginTransmission(int address)
{
    beginTransmission(static_cast<uint8_t>(address));
}

uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
    if (!bus_open_)
    {
        return 4; // other error
    }

    if (!transmitting_)
    {
        return 4; // no active transmission
    }

    /* Check buffer size vs capacity */
    if (txBufferLength_ > LINUX_WIRE_BUFFER_LENGTH)
    {
        resetTxBuffer();
        transmitting_ = false;
        return 1; // data too long
    }

    /* sendStop == 0 means "don't send STOP, prepare for repeated start"
       This allows the next requestFrom() to use a combined write+read
       transaction without an intervening STOP condition. */
    if (sendStop == 0)
    {
        transmitting_ = false;
        hasPendingTxForRead_ = (txBufferLength_ > 0);
        return 0;
    }

    if (!flushPendingRepeatedStart())
    {
        transmitting_ = false;
        return 4;
    }

    /* Nothing queued: the Arduino idiom for "is anyone at this address?".
       lw_write cannot send zero bytes (it returns 0 without touching the
       bus), so probe with an SMBus Quick Write instead. */
    if (txBufferLength_ == 0)
    {
        int rc = lw_probe(&bus_, txAddress_);
        transmitting_ = false;
        resetTxBuffer();
        if (rc >= 0)
        {
            return 0; /* acknowledged, or owned by a kernel driver: present */
        }
        if (errno == ENXIO || errno == EREMOTEIO || errno == EIO)
        {
            return 2; /* NACK on address */
        }
        handleTimeoutFromErrno(); /* ETIMEDOUT sets the timeout flag, as on the write path */
        return 4; /* timeout, adapter cannot probe, or bus error */
    }

    /* Select slave */
    if (lw_set_slave(&bus_, txAddress_) != 0)
    {
        handleTimeoutFromErrno();
        resetTxBuffer();
        transmitting_ = false;
        return 4;
    }

    /* Normal write + STOP (sendStop parameter currently ignored in lw_write) */
    ssize_t written = lw_write(&bus_, txBuffer_, txBufferLength_, 1);
    transmitting_ = false;

    if (written < 0 || static_cast<std::size_t>(written) != txBufferLength_)
    {
        handleTimeoutFromErrno();
        resetTxBuffer();
        return 4;
    }

    resetTxBuffer();
    return 0; // success
}

uint8_t TwoWire::endTransmission(void)
{
    return endTransmission(1);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
{
    const uint8_t *internal = nullptr;
    std::size_t internalLen = 0;
    bool consumePendingTx = false;

    if (hasPendingTxForRead_ && txAddress_ == address)
    {
        internal = txBuffer_;
        internalLen = txBufferLength_;
        consumePendingTx = true;
    }
    else if (hasPendingTxForRead_ && txAddress_ != address)
    {
        if (!flushPendingRepeatedStart())
        {
            resetRxBuffer();
            return 0;
        }
    }

    return requestFrom(address, quantity, internal, internalLen, sendStop, consumePendingTx);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
    return requestFrom(address, quantity, static_cast<uint8_t>(1));
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop)
{
    /* CRITICAL FIX: Validate isize to prevent overflow */
    if (isize > INTERNAL_ADDRESS_MAX)
    {
        isize = INTERNAL_ADDRESS_MAX;
    }

    if (hasPendingTxForRead_ && !flushPendingRepeatedStart())
    {
        resetRxBuffer();
        return 0;
    }

    resetTxBuffer();

    if (isize == 0)
    {
        return requestFrom(address, quantity, nullptr, 0, sendStop, false);
    }

    uint8_t iaddr_buf[INTERNAL_ADDRESS_MAX] = {0};

    /* Convert multi-byte address to big-endian byte array
       e.g., 0x12345678 with isize=4 becomes {0x12, 0x34, 0x56, 0x78} */
    for (uint8_t i = 0; i < isize; ++i)
    {
        const uint8_t shift = (isize - 1U - i) * 8U;
        iaddr_buf[i] = static_cast<uint8_t>((iaddress >> shift) & 0xFF);
    }

    return requestFrom(address, quantity, iaddr_buf, isize, sendStop, false);
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
    if (quantity < 0)
    {
        return 0;
    }
    return requestFrom(static_cast<uint8_t>(address),
                       static_cast<uint8_t>(quantity),
                       static_cast<uint8_t>(1));
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop)
{
    if (quantity < 0)
    {
        return 0;
    }
    return requestFrom(static_cast<uint8_t>(address),
                       static_cast<uint8_t>(quantity),
                       static_cast<uint8_t>(sendStop));
}

size_t TwoWire::write(uint8_t data)
{
    if (!transmitting_)
    {
        /* On Arduino this could be "slave send mode";
           here we do nothing as slave mode is unsupported. */
        return 0;
    }

    if (txBufferLength_ >= LINUX_WIRE_BUFFER_LENGTH)
    {
        return 0;
    }

    txBuffer_[txBufferIndex_] = data;
    ++txBufferIndex_;
    txBufferLength_ = txBufferIndex_;

    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    if (!transmitting_ || !data || quantity == 0)
    {
        return 0;
    }

    /* PERFORMANCE FIX: Optimized bulk write instead of byte-by-byte */
    size_t space = LINUX_WIRE_BUFFER_LENGTH - txBufferLength_;
    size_t to_write = (quantity < space) ? quantity : space;

    if (to_write > 0)
    {
        std::memcpy(&txBuffer_[txBufferIndex_], data, to_write);
        txBufferIndex_ += to_write;
        txBufferLength_ = txBufferIndex_;
    }

    return to_write;
}

size_t TwoWire::write(const char *str)
{
    if (!str)
    {
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(str), std::strlen(str));
}

int TwoWire::available(void) const
{
    assert(rxBufferIndex_ <= rxBufferLength_ && "Buffer index invariant violated");
    return static_cast<int>(rxBufferLength_ - rxBufferIndex_);
}

int TwoWire::read(void)
{
    if (rxBufferIndex_ >= rxBufferLength_)
    {
        return -1;
    }

    int value = rxBuffer_[rxBufferIndex_];
    ++rxBufferIndex_;
    return value;
}

int TwoWire::peek(void)
{
    if (rxBufferIndex_ >= rxBufferLength_)
    {
        return -1;
    }
    return rxBuffer_[rxBufferIndex_];
}

void TwoWire::flush(void)
{
    /* No underlying hardware FIFO in Linux userspace I2C; nothing to do.
       This method exists for Arduino API compatibility only. */
}

void TwoWire::resetTxBuffer()
{
    txBufferIndex_ = 0;
    txBufferLength_ = 0;
    hasPendingTxForRead_ = false;
}

void TwoWire::resetRxBuffer()
{
    rxBufferIndex_ = 0;
    rxBufferLength_ = 0;
}

void TwoWire::applyBusConfiguration()
{
    lw_set_timeout(&bus_, wireTimeoutUs_);
    lw_set_error_logging(&bus_, errorLoggingEnabled_ ? 1 : 0);
}

uint8_t TwoWire::requestFrom(uint8_t address,
                             uint8_t quantity,
                             const uint8_t *internalAddress,
                             std::size_t internalAddressLength,
                             uint8_t sendStop,
                             bool consumePendingTx)
{
    (void)sendStop; /* sendStop is currently ignored. Linux userspace I2C
                       transactions always complete with STOP. The repeated-start
                       behavior is emulated via I2C_RDWR ioctl with combined messages. */

    /* CRITICAL: Validate internal address length to prevent overflow */
    if (internalAddressLength > INTERNAL_ADDRESS_MAX)
    {
        if (consumePendingTx)
        {
            resetTxBuffer();
        }
        return 0;
    }

    if (!bus_open_ || quantity == 0)
    {
        if (consumePendingTx)
        {
            resetTxBuffer();
        }
        return 0;
    }

    if (quantity > LINUX_WIRE_BUFFER_LENGTH)
    {
        quantity = LINUX_WIRE_BUFFER_LENGTH;
    }

    ssize_t result = 0;

    if (internalAddress && internalAddressLength > 0)
    {
        /* Use combined write+read ioctl for repeated-start behavior */
        result = lw_ioctl_read(&bus_, address, internalAddress, internalAddressLength, rxBuffer_, quantity, 0);
        hasPendingTxForRead_ = false;
    }
    else
    {
        /* Standard read: set slave address then read */
        if (lw_set_slave(&bus_, address) != 0)
        {
            handleTimeoutFromErrno();
            resetRxBuffer();
            if (consumePendingTx)
            {
                resetTxBuffer();
            }
            return 0;
        }

        result = lw_read(&bus_, rxBuffer_, quantity);
    }

    if (consumePendingTx)
    {
        resetTxBuffer();
    }

    if (result <= 0)
    {
        handleTimeoutFromErrno();
        resetRxBuffer();
        return 0;
    }

    rxBufferIndex_ = 0;
    rxBufferLength_ = static_cast<std::size_t>(result);
    if (rxBufferLength_ > LINUX_WIRE_BUFFER_LENGTH)
    {
        rxBufferLength_ = LINUX_WIRE_BUFFER_LENGTH;
    }

    return static_cast<uint8_t>(rxBufferLength_);
}

void TwoWire::handleTimeoutFromErrno()
{
    /* Only consider it a timeout if timeout is configured and errno indicates timeout */
    if (wireTimeoutUs_ == 0 || errno != ETIMEDOUT)
    {
        return;
    }

    wireTimeoutFlag_ = true;

    /* Prevent infinite recursion if reopenBus also times out */
    if (wireResetOnTimeout_ && !inTimeoutHandler_)
    {
        inTimeoutHandler_ = true;

        /* Attempt to reopen the bus on timeout if configured */
        if (!reopenBus(devicePath_))
        {
            bus_open_ = false;
        }

        /* Always clean up transaction state */
        resetTxBuffer();
        resetRxBuffer();

        inTimeoutHandler_ = false;
    }
}

bool TwoWire::reopenBus(const char *device)
{
    if (!device || device[0] == '\0')
    {
        bus_open_ = false;
        return false;
    }

    lw_close_bus(&bus_);

    if (lw_open_bus(&bus_, device) == 0)
    {
        bus_open_ = true;
        applyBusConfiguration();
        return true;
    }

    bus_open_ = false;
    return false;
}

bool TwoWire::flushPendingRepeatedStart()
{
    if (!hasPendingTxForRead_)
    {
        return true;
    }

    if (!bus_open_ || txBufferLength_ == 0)
    {
        hasPendingTxForRead_ = false;
        resetTxBuffer();
        return false;
    }

    if (lw_set_slave(&bus_, txAddress_) != 0)
    {
        handleTimeoutFromErrno();
        hasPendingTxForRead_ = false;
        resetTxBuffer();
        return false;
    }

    ssize_t written = lw_write(&bus_, txBuffer_, txBufferLength_, 1);
    hasPendingTxForRead_ = false;

    if (written < 0 || static_cast<std::size_t>(written) != txBufferLength_)
    {
        handleTimeoutFromErrno();
        resetTxBuffer();
        return false;
    }

    resetTxBuffer();
    return true;
}

/* Global instance, matching Arduino Wire API */
TwoWire Wire;
