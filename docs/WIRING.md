# Prototype wiring

> **BETA / TESTING** - verify the exact pinout and voltage limits of the RF module you own before powering the prototype.

## ESP32 DevKit + FS1000A + BME280 + analog UV

| ESP32 | Device | Signal |
|---|---|---|
| GPIO25 | FS1000A | DATA |
| GND | FS1000A | GND |
| module-rated supply | FS1000A | VCC |
| GPIO21 | BME280 | SDA |
| GPIO22 | BME280 | SCL |
| 3.3 V | BME280 | VCC |
| GND | BME280 | GND |
| GPIO34 | UV module | analog output |
| 3.3 V / module-rated | UV module | VCC |
| GND | UV module | GND |

All grounds must be common.

## ESP32-C3 default beta mapping

| ESP32-C3 | Function |
|---|---|
| GPIO4 | RF DATA |
| GPIO6 | I2C SDA |
| GPIO7 | I2C SCL |
| GPIO0 | UV ADC |
| GPIO8 | status LED |

These values are compile-time defaults in `include/app_config.h` and can be changed there.

## RF antenna

For first bench tests use a straight approximately 17.3 cm wire as a quarter-wave reference antenna for 433 MHz. Keep it away from the ESP32 antenna, USB cable and large ground planes while comparing range.

## FS1000A supply note

Cheap OOK transmitter modules vary substantially. Many operate from a higher supply voltage than the ESP32 while still accepting a 3.3 V DATA high level, but this must not be assumed for every clone. Check the module data or measure the input stage. Do not apply 5 V to an ESP32 GPIO.

## First power-up

1. Leave RF disabled in the Web UI if uncertain about the TX module.
2. Verify BME280 readings.
3. Verify the UV ADC reading and calibration controls.
4. Verify the frame preview changes correctly when temperature/humidity change.
5. Enable RF and inspect GPIO DATA with a logic analyzer.
6. Only then connect the RF module and move to the LILYGO reception test.
