/*
 * C example: i2c_scanner
 * Mirrors examples/cpp/i2c_scanner/main.cpp but uses the C API (linux_wire.h)
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "linux_wire.h"

int main(void)
{
    lw_i2c_bus bus;
    if (lw_open_bus(&bus, "/dev/i2c-1") != 0) {
        fprintf(stderr, "Failed to open /dev/i2c-1\n");
        return 1;
    }

    printf("Scanning I2C bus /dev/i2c-1...\n");

    /* An adapter without SMBus Quick makes every probe fail the same way;
       say so once instead of printing an empty scan. */
    if (lw_probe(&bus, 0x03) < 0 && errno == EOPNOTSUPP) {
        fprintf(stderr, "Adapter does not support SMBus Quick Write; cannot probe\n");
        lw_close_bus(&bus);
        return 1;
    }

    for (int addr = 0x03; addr <= 0x77; ++addr)
    {
        /* Address-only probe (SMBus Quick Write): nothing is read from or
           written to the device, unlike a one-byte read, which advances a
           register pointer or consumes a pending response. */
        int rc = lw_probe(&bus, (uint8_t)addr);
        if (rc == 0) {
            printf("Found device at 0x%02X\n", addr);
        } else if (rc == 1) {
            printf("Found device at 0x%02X (in use by a kernel driver)\n", addr);
        }
    }

    lw_close_bus(&bus);
    return 0;
}
