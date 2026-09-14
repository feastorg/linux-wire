#include "mock_linux_wire.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

extern "C"
{
#include "linux_wire.h"
}

namespace
{
    struct MockConfig
    {
        std::vector<uint8_t> readData;
        std::vector<uint8_t> ioctlReadData;
        bool failRead = false;
        int failReadErrno = ETIMEDOUT;
        bool failSetTarget = false;
        int failSetTargetErrno = ENXIO;
        bool failWrite = false;
        int failWriteErrno = EIO;
        int probeResult = 0;
        int probeErrno = ENXIO;
    };

    MockLinuxWireState g_state;
    MockConfig g_config;
} // namespace

void mockLinuxWireReset()
{
    g_state = MockLinuxWireState{};
    g_config = MockConfig{};
}

void mockLinuxWireSetProbeResult(int result, int err)
{
    g_config.probeResult = result;
    g_config.probeErrno = err;
}

void mockLinuxWireSetReadData(const std::vector<uint8_t> &data)
{
    g_config.readData = data;
}

void mockLinuxWireSetIoctlReadData(const std::vector<uint8_t> &data)
{
    g_config.ioctlReadData = data;
}

void mockLinuxWireForceReadError(int err)
{
    g_config.failRead = true;
    g_config.failReadErrno = err;
}

void mockLinuxWireClearReadError()
{
    g_config.failRead = false;
}

void mockLinuxWireForceSetTargetError(int err)
{
    g_config.failSetTarget = true;
    g_config.failSetTargetErrno = err;
}

void mockLinuxWireClearSetTargetError()
{
    g_config.failSetTarget = false;
}

void mockLinuxWireForceWriteError(int err)
{
    g_config.failWrite = true;
    g_config.failWriteErrno = err;
}

void mockLinuxWireClearWriteError()
{
    g_config.failWrite = false;
}

const MockLinuxWireState &mockLinuxWireState()
{
    return g_state;
}

extern "C"
{
    int lw_open_bus(lw_i2c_bus *bus, const char *device_path)
    {
        ++g_state.openCalls;
        if (!bus || !device_path || device_path[0] == '\0')
        {
            errno = EINVAL;
            return -1;
        }

        bus->fd = 1;
        std::strncpy(bus->device_path, device_path, LINUX_WIRE_DEVICE_PATH_MAX - 1);
        bus->device_path[LINUX_WIRE_DEVICE_PATH_MAX - 1] = '\0';
        bus->timeout_us = 0;
        bus->log_errors = 1;
        g_state.lastDevicePath = device_path;
        g_state.lastTimeoutUs = 0;
        g_state.logErrors = 1;
        return 0;
    }

    void lw_close_bus(lw_i2c_bus *bus)
    {
        ++g_state.closeCalls;
        if (bus)
        {
            bus->fd = -1;
            bus->device_path[0] = '\0';
        }
    }

    int lw_set_target(lw_i2c_bus * /*bus*/, uint8_t addr)
    {
        ++g_state.setTargetCalls;
        g_state.lastSetTargetAddr = addr;
        if (g_config.failSetTarget)
        {
            errno = g_config.failSetTargetErrno;
            return -1;
        }
        return 0;
    }

    int lw_probe(lw_i2c_bus * /*bus*/, uint8_t addr)
    {
        ++g_state.probeCalls;
        g_state.lastProbeAddr = addr;
        if (g_config.probeResult < 0)
        {
            errno = g_config.probeErrno;
        }
        return g_config.probeResult;
    }

ssize_t lw_write(lw_i2c_bus * /*bus*/, const uint8_t *data, size_t len, int /*send_stop*/)
{
    ++g_state.writeCalls;
    g_state.lastWriteWasIoctl = false;
    g_state.lastWriteTargetAddr = g_state.lastSetTargetAddr;
    if (g_config.failWrite)
    {
        errno = g_config.failWriteErrno;
        return -1;
    }
    g_state.lastWriteBuffer.assign(data, data + len);
    return static_cast<ssize_t>(len);
}

    ssize_t lw_read(lw_i2c_bus * /*bus*/, uint8_t *data, size_t len)
    {
        ++g_state.readCalls;
        if (g_config.failRead)
        {
            errno = g_config.failReadErrno;
            return -1;
        }

        const size_t to_copy = std::min(len, g_config.readData.size());
        if (to_copy > 0)
        {
            std::memcpy(data, g_config.readData.data(), to_copy);
        }
        else if (len > 0)
        {
            data[0] = 0;
        }
        g_state.lastReadBuffer.assign(data, data + to_copy);
        return static_cast<ssize_t>(to_copy);
    }

    ssize_t lw_ioctl_read(lw_i2c_bus * /*bus*/,
                          uint16_t addr,
                          const uint8_t *iaddr,
                          size_t iaddr_len,
                          uint8_t *data,
                          size_t len,
                          uint16_t /*flags*/)
    {
        ++g_state.ioctlReadCalls;
        g_state.lastIoctlAddr = addr;
        g_state.lastIoctlInternal.assign(iaddr, iaddr + iaddr_len);

        const size_t to_copy = std::min(len, g_config.ioctlReadData.size());
        if (to_copy > 0)
        {
            std::memcpy(data, g_config.ioctlReadData.data(), to_copy);
        }
        return static_cast<ssize_t>(to_copy);
    }

ssize_t lw_ioctl_write(lw_i2c_bus * /*bus*/,
                       uint16_t addr,
                       const uint8_t *iaddr,
                       size_t iaddr_len,
                       const uint8_t *data,
                       size_t len,
                       uint16_t /*flags*/)
{
    ++g_state.writeCalls;
    g_state.lastSetTargetAddr = static_cast<uint8_t>(addr);
    g_state.lastWriteBuffer.assign(iaddr, iaddr + iaddr_len);
    g_state.lastWriteBuffer.insert(g_state.lastWriteBuffer.end(), data, data + len);
    g_state.lastWriteWasIoctl = true;
    g_state.lastWriteTargetAddr = static_cast<uint8_t>(addr);
    return static_cast<ssize_t>(len);
}

int lw_set_timeout(lw_i2c_bus *bus, uint32_t timeout_us)
{
    ++g_state.setTimeoutCalls;
    g_state.lastTimeoutUs = timeout_us;
    if (bus)
    {
        bus->timeout_us = timeout_us;
    }
    return 0;
}

void lw_set_error_logging(lw_i2c_bus *bus, int enable)
{
    ++g_state.setErrorLoggingCalls;
    g_state.logErrors = enable ? 1 : 0;
    if (bus)
    {
        bus->log_errors = g_state.logErrors;
    }
}

} // extern "C"
