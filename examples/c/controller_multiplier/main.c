/*
 * C example: controller_multiplier
 * Sends a byte to a device and reads the response (expected multiply result).
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "linux_wire.h"

static const uint16_t DEVICE_ADDR = 0x40;

int main(void)
{
    lw_i2c_bus bus;
    if (lw_open_bus(&bus, "/dev/i2c-1") != 0) {
        fprintf(stderr, "Failed to open /dev/i2c-1\n");
        return 1;
    }

    printf("Pi -> Device multiplier test\n");

    uint8_t x = 7;
    printf("Sending X=%u\n", x);

    /* Send single byte (payload only) */
    ssize_t w = lw_ioctl_write(&bus, DEVICE_ADDR, NULL, 0, &x, 1, 0);
    if (w != 1) {
        perror("Write failed");
        lw_close_bus(&bus);
        return 1;
    }

    /* sleep ~1ms */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000 * 1000 };
    nanosleep(&ts, NULL);

    uint8_t result = 0;
    ssize_t r = lw_ioctl_read(&bus, DEVICE_ADDR, NULL, 0, &result, 1, 0);
    if (r != 1) {
        fprintf(stderr, "Read failed\n");
        lw_close_bus(&bus);
        return 1;
    }

    printf("Received R=%u (expected %u)\n", result, x * 5);

    lw_close_bus(&bus);
    return 0;
}
