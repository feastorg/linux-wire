#include <cstdio>
#include "Wire.h"

// Writes a single byte (or sequence of bytes) to an I2C device.
// Adjust DEVICE_ADDR and REGISTER_ADDR as needed.

static constexpr uint8_t DEVICE_ADDR = 0x40;   // Example device address
static constexpr uint8_t REGISTER_ADDR = 0x00; // Example register

int main()
{
    Wire.begin("/dev/i2c-1");

    printf("Controller Writer Example\n");

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(REGISTER_ADDR);
    Wire.write(0xAB); // Example data byte
    uint8_t result = Wire.endTransmission();

    if (result == 0)
    {
        printf("Write OK\n");
    }
    else
    {
        printf("Write error: %u\n", result);
    }

    return 0;
}
