/*
 * C example: controller_reader
 * Reads a single register from an I2C device using a repeated-start (ioctl) read.
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

    printf("Controller Reader Example\n");

    uint8_t value = 0;
    ssize_t r = lw_ioctl_read(&bus, DEVICE_ADDR, &REGISTER_ADDR, 1, &value, 1, 0);
    if (r != 1) {
        fprintf(stderr, "Read failed\n");
        lw_close_bus(&bus);
        return 1;
    }

    printf("Read value: 0x%02X (%d)\n", value, value);

    lw_close_bus(&bus);
    return 0;
}
