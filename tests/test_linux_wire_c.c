#include "linux_wire.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define EXPECT_ERR(call, err)       \
    do                              \
    {                               \
        errno = 0;                  \
        assert((call) == -1);       \
        assert(errno == (err));     \
    } while (0)

int main(void)
{
    lw_i2c_bus bus;
    memset(&bus, 0, sizeof(bus));
    bus.fd = -1;
    bus.log_errors = 1;

    EXPECT_ERR(lw_open_bus(NULL, "/dev/null"), EINVAL);
    EXPECT_ERR(lw_open_bus(&bus, ""), EINVAL);
    assert(bus.fd == -1);
    assert(bus.device_path[0] == '\0');
    assert(bus.timeout_us == 0);

    bus.fd = 42;
    memset(bus.device_path, 'x', sizeof(bus.device_path));
    bus.timeout_us = 99;
    EXPECT_ERR(lw_open_bus(&bus, "/dev/not-i2c-1"), EINVAL);
    assert(bus.fd == -1);
    assert(bus.device_path[0] == '\0');
    assert(bus.timeout_us == 0);

    bus.fd = 42;
    memset(bus.device_path, 'x', sizeof(bus.device_path));
    bus.timeout_us = 99;
    EXPECT_ERR(lw_open_bus(&bus, "/dev/i2c-1x"), EINVAL);
    assert(bus.fd == -1);
    assert(bus.device_path[0] == '\0');
    assert(bus.timeout_us == 0);

    EXPECT_ERR(lw_set_target(&bus, 0x10), EBADF);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    EXPECT_ERR(lw_set_slave(&bus, 0x10), EBADF); /* deprecated alias still works */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_ERR(lw_probe(&bus, 0x10), EBADF);

    uint8_t byte = 0x00;
    uint8_t buf[1];

    EXPECT_ERR(lw_write(&bus, &byte, 1, 1), EBADF);
    EXPECT_ERR(lw_read(&bus, buf, 1), EBADF);

    EXPECT_ERR(lw_ioctl_read(&bus, 0x10, &byte, 1, buf, 1, 0), EBADF);
    EXPECT_ERR(lw_ioctl_write(&bus, 0x10, &byte, 1, &byte, 1, 0), EBADF);

    bus.fd = 0; /* bypass fd check to hit other validation branches */
    EXPECT_ERR(lw_set_target(&bus, 0x80), EINVAL);
    EXPECT_ERR(lw_probe(&bus, 0x80), EINVAL);
    EXPECT_ERR(lw_probe(NULL, 0x10), EINVAL);

    EXPECT_ERR(lw_ioctl_write(&bus, 0x20, NULL, 1, &byte, 1, 0), EINVAL);
    EXPECT_ERR(lw_ioctl_write(&bus, 0x20, &byte, 1, NULL, 1, 0), EINVAL);
    EXPECT_ERR(lw_ioctl_write(&bus, 0x20, NULL, 0, NULL, 0, 0), EINVAL);

    /* A real fd that is not an I2C adapter: the I2C_SLAVE ioctl fails with
       ENOTTY, proving lw_probe reaches the ioctl and propagates errno. A pipe
       needs no device node, so this holds in any sandbox. */
    {
        int fds[2];
        assert(pipe(fds) == 0);
        bus.fd = fds[1];
        bus.log_errors = 1; /* and nothing may be printed: lw_probe never logs */
        EXPECT_ERR(lw_probe(&bus, 0x10), ENOTTY);
        close(fds[0]);
        close(fds[1]);
        bus.fd = 0;
    }

    lw_set_error_logging(&bus, 0);
    assert(bus.log_errors == 0);
    lw_set_error_logging(&bus, 1);
    assert(bus.log_errors == 1);

    return 0;
}
