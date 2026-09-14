#include "linux_wire.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

/* Stack buffer size for small I2C transfers to avoid heap allocation */
#define LW_STACK_BUFFER_SIZE 256

/* Maximum payload for ioctl operations */
#define LW_MAX_IOCTL_PAYLOAD 4096

static void lw_reset_bus_handle(lw_i2c_bus *bus)
{
    if (!bus)
    {
        return;
    }

    bus->fd = -1;
    bus->device_path[0] = '\0';
    bus->timeout_us = 0;
    bus->log_errors = 1;
}

int lw_open_bus(lw_i2c_bus *bus, const char *device_path)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    lw_reset_bus_handle(bus);

    if (!device_path || device_path[0] == '\0')
    {
        errno = EINVAL;
        return -1;
    }

    /* Security: Validate that path points to an I2C device */
    if (strncmp(device_path, "/dev/i2c-", 9) != 0)
    {
        errno = EINVAL;
        return -1;
    }

    /* Additional security: Verify remainder is only digits */
    const char *p = device_path + 9;
    if (!*p)
    {
        errno = EINVAL;
        return -1;
    }

    while (*p)
    {
        if (*p < '0' || *p > '9')
        {
            errno = EINVAL;
            return -1;
        }
        p++;
    }

    int fd = open(device_path, O_RDWR);

    if (fd < 0)
    {
        int saved_errno = errno;
        perror("lw_open_bus: open");
        errno = saved_errno;
        return -1;
    }

    bus->fd = fd;

    /* Efficient string copy with proper bounds checking */
    size_t path_len = strlen(device_path);
    if (path_len >= sizeof(bus->device_path))
    {
        path_len = sizeof(bus->device_path) - 1;
    }
    memcpy(bus->device_path, device_path, path_len);
    bus->device_path[path_len] = '\0';

    bus->timeout_us = 0;
    bus->log_errors = 1;

    return 0;
}

void lw_close_bus(lw_i2c_bus *bus)
{
    if (!bus)
    {
        return;
    }
    if (bus->fd >= 0)
    {
        close(bus->fd);
        bus->fd = -1;
    }
    bus->device_path[0] = '\0';
    bus->timeout_us = 0;
}

int lw_set_slave(lw_i2c_bus *bus, uint8_t addr)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (addr > 0x7F)
    {
        errno = EINVAL;
        return -1;
    }

    if (ioctl(bus->fd, I2C_SLAVE, addr) < 0)
    {
        int saved_errno = errno;
        if (bus->log_errors)
        {
            perror("lw_set_slave: I2C_SLAVE");
        }
        errno = saved_errno;
        return -1;
    }

    return 0;
}

int lw_probe(lw_i2c_bus *bus, uint8_t addr)
{
    if (!bus || addr > 0x7F)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (lw_set_slave(bus, addr) != 0)
    {
        return -1; /* errno from the I2C_SLAVE ioctl (EBUSY if a driver owns it) */
    }

    /* SMBus Quick Write: address + write bit, then STOP. No data phase. */
    struct i2c_smbus_ioctl_data args;
    memset(&args, 0, sizeof(args));
    args.read_write = I2C_SMBUS_WRITE;
    args.command = 0;
    args.size = I2C_SMBUS_QUICK;
    args.data = NULL;

    if (ioctl(bus->fd, I2C_SMBUS, &args) < 0)
    {
        /* A NACK is the expected outcome for an empty address; do not log it
           through perror, callers scan whole ranges with this. */
        return -1;
    }

    return 0;
}

ssize_t lw_write(lw_i2c_bus *bus,
                 const uint8_t *data,
                 size_t len,
                 int send_stop)
{
    (void)send_stop; /* Currently ignored: each write issues a STOP.
                        Linux userspace I2C doesn't provide fine-grained
                        control over STOP conditions via write() */

    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (!data && len > 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (len == 0)
    {
        return 0;
    }

    ssize_t written = write(bus->fd, data, len);
    if (written < 0)
    {
        int saved_errno = errno;
        if (bus->log_errors)
        {
            perror("lw_write: write");
        }
        errno = saved_errno;
    }
    return written;
}

ssize_t lw_read(lw_i2c_bus *bus,
                uint8_t *data,
                size_t len)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (!data && len > 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (len == 0)
    {
        return 0;
    }

    ssize_t r = read(bus->fd, data, len);
    if (r < 0)
    {
        int saved_errno = errno;
        if (bus->log_errors)
        {
            perror("lw_read: read");
        }
        errno = saved_errno;
    }
    return r;
}

ssize_t lw_ioctl_read(lw_i2c_bus *bus,
                      uint16_t addr,
                      const uint8_t *iaddr,
                      size_t iaddr_len,
                      uint8_t *data,
                      size_t len,
                      uint16_t flags)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (!data || len == 0 || (iaddr_len > 0 && !iaddr))
    {
        errno = EINVAL;
        return -1;
    }

    /* Validate sizes before any operations */
    if (iaddr_len > UINT16_MAX || len > UINT16_MAX)
    {
        errno = EINVAL;
        return -1;
    }

    struct i2c_msg msgs[2] = {{0}};
    struct i2c_rdwr_ioctl_data rdwr = {0};

    int msg_count = 0;

    if (iaddr && iaddr_len > 0)
    {
        /* Write internal address first */
        msgs[msg_count].addr = addr;
        msgs[msg_count].flags = flags;
        /* i2c_msg.buf is non-const in kernel API */
        msgs[msg_count].buf = (uint8_t *)iaddr;
        msgs[msg_count].len = (uint16_t)iaddr_len;
        ++msg_count;
    }

    /* Read message */
    msgs[msg_count].addr = addr;
    msgs[msg_count].flags = flags | I2C_M_RD;
    msgs[msg_count].buf = data;
    msgs[msg_count].len = (uint16_t)len;
    ++msg_count;

    rdwr.msgs = msgs;
    rdwr.nmsgs = msg_count;

    if (ioctl(bus->fd, I2C_RDWR, &rdwr) < 0)
    {
        int saved_errno = errno;
        if (bus->log_errors)
        {
            perror("lw_ioctl_read: I2C_RDWR");
        }
        errno = saved_errno;
        return -1;
    }

    return (ssize_t)len;
}

ssize_t lw_ioctl_write(lw_i2c_bus *bus,
                       uint16_t addr,
                       const uint8_t *iaddr,
                       size_t iaddr_len,
                       const uint8_t *data,
                       size_t len,
                       uint16_t flags)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }

    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if ((len > 0 && !data) ||
        (iaddr_len > 0 && !iaddr) ||
        (iaddr_len + len == 0))
    {
        errno = EINVAL;
        return -1;
    }

    /* CRITICAL FIX: Validate sizes BEFORE any calculations to prevent overflow */
    if (iaddr_len > UINT16_MAX || len > UINT16_MAX)
    {
        errno = EINVAL;
        return -1;
    }

    /* Check for addition overflow */
    if (len > SIZE_MAX - iaddr_len)
    {
        errno = EOVERFLOW;
        return -1;
    }

    size_t total_len = iaddr_len + len;

    /* Validate against both UINT16_MAX and reasonable payload size */
    if (total_len > UINT16_MAX || total_len > LW_MAX_IOCTL_PAYLOAD)
    {
        errno = EINVAL;
        return -1;
    }

    /* PERFORMANCE FIX: Use stack allocation for small buffers */
    uint8_t stack_buf[LW_STACK_BUFFER_SIZE];
    uint8_t *buf;
    int heap_allocated = 0;

    if (total_len <= LW_STACK_BUFFER_SIZE)
    {
        buf = stack_buf;
    }
    else
    {
        buf = (uint8_t *)malloc(total_len);
        if (!buf)
        {
            errno = ENOMEM;
            return -1;
        }
        heap_allocated = 1;
    }

    /* Build combined buffer: [iaddr (optional)] [data] */
    if (iaddr && iaddr_len > 0)
        memcpy(buf, iaddr, iaddr_len);
    if (data && len > 0)
        memcpy(buf + iaddr_len, data, len);

    struct i2c_msg msg = {0};
    struct i2c_rdwr_ioctl_data rdwr = {0};

    msg.addr = addr;
    msg.flags = flags;
    msg.buf = buf;
    msg.len = (uint16_t)total_len;

    rdwr.msgs = &msg;
    rdwr.nmsgs = 1;

    if (ioctl(bus->fd, I2C_RDWR, &rdwr) < 0)
    {
        int saved_errno = errno;
        if (bus->log_errors)
        {
            perror("lw_ioctl_write: I2C_RDWR");
        }
        if (heap_allocated)
        {
            free(buf);
        }

        errno = saved_errno; /* Restore errno AFTER free() */
        return -1;
    }

    if (heap_allocated)
    {
        free(buf);
    }

    return (ssize_t)len;
}

int lw_set_timeout(lw_i2c_bus *bus, uint32_t timeout_us)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }
    bus->timeout_us = timeout_us;
    /* Currently no enforcement; placeholder for future poll/select logic. */
    return 0;
}

void lw_set_error_logging(lw_i2c_bus *bus, int enable)
{
    if (!bus)
    {
        return;
    }
    bus->log_errors = enable ? 1 : 0;
}
