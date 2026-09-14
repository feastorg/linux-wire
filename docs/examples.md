# Examples

## Arduino Peripheral Companion

Use this sketch to emulate a simple I²C peripheral that works with the `linux-wire` examples (`i2c_scanner`, `controller_reader`, `controller_writer`, `controller_multiplier`). Flash it onto an Arduino (Nano/Uno) and connect it to your Raspberry Pi via a bidirectional level shifter.

### Wiring

| Raspberry Pi (3.3V) | Level Shifter | Arduino (5V) |
| ------------------- | ------------- | ------------ |
| 3.3V                | HV 5V         | 5V           |
| SDA (GPIO 2)        | SDA           | A4           |
| SCL (GPIO 3)        | SCL           | A5           |
| GND                 | GND           | GND          |

Keep the grounds common across the Pi, shifter, and Arduino.

### Sketch

```cpp
#include <Wire.h>

static const uint8_t DEVICE_ADDRESS = 0x40;
static volatile uint8_t registerValue = 0x00;

void receiveEvent(int numBytes)
{
    if (numBytes >= 1)
    {
        registerValue = Wire.read();

        while (Wire.available())
        {
            Wire.read();
        }
    }
}

void requestEvent()
{
    Wire.write(registerValue);
}

void setup()
{
    Wire.begin(DEVICE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

void loop()
{
    // callbacks handle all traffic
}
```

This mirrors the stock Arduino `Wire` peripheral examples: whatever byte the controller last wrote will be returned on the next read.

### Test Flow

1. Flash the sketch and connect the Nano to the Pi via the level shifter.
2. On the Pi (with `linux-wire` built):

   ```sh
   cd build
   sudo ./i2c_scanner            # should report the device at 0x40
   sudo ./controller_writer          # writes a test pattern to the Nano
   sudo ./controller_reader          # reads it back
   sudo ./controller_multiplier      # optional rolling pattern test
   ```

   Each binary accepts optional command-line arguments (`--help`) if you need to change bus, address, or payload.

3. Use the Arduino Serial Monitor at 115200 baud if you’d like to watch activity while debugging (optional).

This loopback peripheral gives you deterministic end-to-end coverage without relying on external sensors.\*\*\*
