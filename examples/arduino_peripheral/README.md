# Arduino peripheral examples

## Purpose

These sketches turn an Arduino (or compatible MCU board) into a simple I2C peripheral so the `examples/*` programs can be tested using real hardware.

## What’s here

- `arduino_simple_peripheral/` — maintains a single byte register that can be written/read via I2C.
- `arduino_multiplier_peripheral/` — simple demo: writes an input X, stores R = X\*5, serves R on request.

## Safety & wiring

- Connect SDA, SCL and GND between your host SBC (Raspberry Pi etc.) and the Arduino board. DO NOT connect 3.3V <-> 5V without level shifting if your host uses 3.3V.
- Sketches use `Wire` library and expect peripheral address `0x40` by default — adjust as needed.

## How to use

1. Flash one of the sketches to your Arduino.
2. Wire: connect GND to GND, SDA to SDA, SCL to SCL on the host.
3. Run the host-side example that matches the sketch (e.g. `controller_multiplier_*`) and set the bus path if necessary: e.g. `Wire.begin("/dev/i2c-1")` in the example binaries.

## Notes

- These sketches are examples only — use real device datasheets and level-shifting practices in production setups.
