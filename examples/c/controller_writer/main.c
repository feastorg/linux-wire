/*
 * C example: controller_writer
 * Writes a register and a value to an I2C device using the C API.
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "linux_wire.h"

static const uint16_t DEVICE_ADDR = 0x40;
static const uint8_t REGISTER_ADDR = 0x00;

int main(void)
{
    lw_i2c_bus bus;
    if (lw_open_bus(&bus, "/dev/i2c-1") != 0) {
        fprintf(stderr, "Failed to open /dev/i2c-1\n");
        return 1;
    }

    printf("Controller Writer Example\n");

    uint8_t value = 0xAB;
    ssize_t r = lw_ioctl_write(&bus, DEVICE_ADDR, &REGISTER_ADDR, 1, &value, 1, 0);
    if (r < 0) {
        perror("Write failed");
        lw_close_bus(&bus);
        return 1;
    }

    printf("Write OK\n");

    lw_close_bus(&bus);
    return 0;
}
