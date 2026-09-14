#include <Wire.h>

static const uint8_t DEVICE_ADDRESS = 0x40;
static volatile uint8_t registerValue = 0x00;

void receiveEvent(int numBytes)
{
    if (numBytes >= 1)
    {
        uint8_t incoming = Wire.read();
        registerValue = incoming;

        Serial.print("W: ");
        Serial.println(incoming, HEX);

        while (Wire.available())
        {
            Wire.read(); // drain remaining bytes
        }
    }
}

void requestEvent()
{
    Wire.write(registerValue);

    Serial.print("R: ");
    Serial.println(registerValue, HEX);
}

void setup()
{
    Serial.begin(115200);
    delay(250); // allow serial connection to settle

    Wire.begin(DEVICE_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

    Serial.print("I2C Peripheral ready at 0x");
    Serial.println(DEVICE_ADDRESS, HEX);
}

void loop()
{
    // all work is done in callbacks
}
