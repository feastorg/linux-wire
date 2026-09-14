/*
 *
 * Cpp example: i2c_scanner_strict
 *
 * A stricter I2C scanner for Linux. It writes one dummy byte before STOP,
 *
 * ensuring a real data-phase ACK instead of accepting address-only ACKs.
 *
 * This prevents false positives from devices that ACK address probes but
 *
 * would NACK real writes (e.g., AVR/ATmega Wire peripherals).
 *
 * Useful when scanning buses with AVR Wire devices or when verifying that
 *
 * a device acknowledges both the address and the data phase.
 */

#include <cstdio>
#include "Wire.h"

int main()
{
    Wire.begin("/dev/i2c-1");
    Wire.setErrorLogging(false);

    printf("Strict scanning I2C bus /dev/i2c-1...\n");

    uint8_t dummy = 0x00;

    for (int address = 0x03; address <= 0x77; ++address)
    {
        Wire.beginTransmission(address);
        Wire.write(dummy); // force a real write; prevents AVR ACK storms
        uint8_t error = Wire.endTransmission();

        if (error == 0)
        {
            printf("Found device at 0x%02X\n", address);
        }
    }

    return 0;
}
