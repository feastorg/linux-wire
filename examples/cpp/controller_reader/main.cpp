#include <cstdio>
#include "Wire.h"

// Reads a single register from an I2C device.
// Adjust DEVICE_ADDR and REGISTER_ADDR as needed.

static constexpr uint8_t DEVICE_ADDR = 0x40;
static constexpr uint8_t REGISTER_ADDR = 0x00;

int main()
{
    Wire.begin("/dev/i2c-1");

    printf("Controller Reader Example\n");

    // Write the register address we want to read
    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(REGISTER_ADDR);
    uint8_t result = Wire.endTransmission(0); // request a repeated-start style read

    if (result != 0)
    {
        printf("Write phase failed: %u\n", result);
        return 1;
    }

    // Request 1 byte back - this will use a combined ioctl read when the TX buffer
    // was left in place by endTransmission(0)
    uint8_t count = Wire.requestFrom(DEVICE_ADDR, (uint8_t)1);
    if (count == 0 || !Wire.available())
    {
        printf("Read failed\n");
        return 1;
    }

    int value = Wire.read();
    printf("Read value: 0x%02X (%d)\n", value & 0xFF, value);

    return 0;
}
