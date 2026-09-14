#include <cstdio>
#include <thread>
#include <chrono>
#include "Wire.h"

/*
Expected output:
```
Pi → Nano Multiplier Test
Sending X=7
Received R=35 (expected 35)
```
*/

static constexpr uint8_t DEVICE_ADDR = 0x40;

int main()
{
    Wire.begin("/dev/i2c-1");

    printf("Pi → Nano Multiplier Test\n");

    // Send a test value
    uint8_t x = 7; // Try changing this number
    printf("Sending X=%u\n", x);

    Wire.beginTransmission(DEVICE_ADDR);
    Wire.write(x);
    uint8_t err = Wire.endTransmission();
    if (err != 0)
    {
        printf("Write error: %u\n", err);
        return 1;
    }

    // Wait briefly to allow the Nano to process the value (~1ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Request 1 byte back
    uint8_t count = Wire.requestFrom(DEVICE_ADDR, (uint8_t)1);
    if (count != 1 || !Wire.available())
    {
        printf("Read failed\n");
        return 1;
    }

    uint8_t result = Wire.read();
    printf("Received R=%u (expected %u)\n", result, x * 5);

    return 0;
}
