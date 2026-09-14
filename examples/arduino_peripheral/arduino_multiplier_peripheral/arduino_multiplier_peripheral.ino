#include <Wire.h>

/*
Expected output:
```
HH:MM:SS.mmm -> I2C Multiplier Peripheral at 0x40
HH:MM:SS.mmm -> RX X=7  →  stored R=35
HH:MM:SS.mmm -> TX R=35
```
*/

static const uint8_t DEVICE_ADDRESS = 0x40;

// Holds latest multiplied result (X * 5)
static volatile uint8_t processedValue = 0x00;

void receiveEvent(int numBytes)
{
    if (numBytes < 1)
        return;

    uint8_t incoming = Wire.read();

    // Process the value (multiply by 5)
    processedValue = incoming * 5;

    Serial.print("RX X=");
    Serial.print(incoming);
    Serial.print("  →  stored R=");
    Serial.println(processedValue);

    // Drain any remaining bytes if present
    while (Wire.available())
        Wire.read();
}

void requestEvent()
{
    Wire.write(processedValue);

    Serial.print("TX R=");
    Serial.println(processedValue);
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    Serial.print("I2C Multiplier Peripheral at 0x");
    Serial.println(DEVICE_ADDRESS, HEX);

    Wire.begin(DEVICE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

void loop()
{
    // Nothing to do; callbacks handle all logic
}
