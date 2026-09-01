# Oregon Scientific V3 transmission notes

> Project status: **BETA / TESTING**. These mappings must be validated on the companion LILYGO receiver and on original WMR88/WMR200 consoles.

## Design rule

The transmitter does not define a private over-the-air protocol. Every virtual device is encoded as an Oregon Scientific compatible sensor. The companion gateway therefore receives the exact same RF frame as an original Oregon console.

## RF layer

Initial beta parameters:

- center frequency: determined by the external 433.92 MHz ASK/OOK module
- modulation: OOK/ASK
- OSV3 nominal half-bit: 488 us
- Manchester encoding
- 24 logical `1` preamble bits
- sync sequence `0101`
- payload nibbles transmitted least-significant bit first
- polarity selectable because low-cost OOK modules can invert the baseband path

The `halfUs` value is intentionally configurable from 430 to 550 us so it can be tuned from measurements against genuine sensors.

## Internal frame representation

For consistency with `esp32-oregon-technoline-weather-gateway`, the encoder stores a synthetic sync nibble `A` at nibble 0. The Oregon sensor ID starts at nibble 1.

Common field mapping used by the beta encoder:

| Nibble | Meaning |
|---:|---|
| 0 | synthetic sync `A` - not emitted as payload |
| 1..4 | Oregon sensor code |
| 5 | channel raw value (`1`, `2`, `4`) |
| 6..7 | rolling ID |
| 8 | flags where supported |
| 9.. | sensor-specific data |

## Implemented virtual profiles

### THGR810 / THGN801 - `F824`

Primary hardware-validation profile.

- length in gateway representation: 9 bytes
- temperature resolution: 0.1 C
- humidity resolution: 1 %
- checksum begins at nibble 16
- supported console channel values: 1-3

Temperature payload follows the companion gateway decoder:

- nibble 9: tenths
- nibble 10: ones
- nibble 11: tens
- nibble 12: sign (`8` negative, `0` positive)
- nibble 13: humidity ones
- nibble 14: humidity tens
- nibble 15: reserved/status in beta

### UVN800 - `D874`

Second hardware-validation profile.

- length: 8 bytes
- UV index encoded as unsigned integer in byte 4 of the companion gateway representation
- checksum begins at nibble 14

The low-battery flag is intentionally not asserted in the beta UV encoder until behavior is confirmed from genuine captures.

### PCR800 - `2914`

Encoder is present for bench work but is **not yet declared console-validated**.

Current project mapping follows the companion gateway parser:

- rain rate raw step: 0.254 mm/h
- total rain compatibility raw step: 0.254 mm
- checksum starts at nibble 19

This profile must be verified with raw captures before field deployment.

### WGR800 - `1984`

Encoder is present for bench work but is **not yet declared console-validated**.

- direction index: 0..15
- average and gust speed represented as m/s with 0.1 m/s BCD resolution, converted from km/h by the firmware API
- checksum starts at nibble 18

## Checksum

The current companion gateway validates these OSV3 frames using an 8-bit sum of payload nibbles from nibble 1 up to the nibble before the checksum, with checksum low nibble transmitted first.

That exact convention is used here so the first validation step can compare TX and RX byte-for-byte.

## Cadence

Cadence matters to original Oregon consoles. For beta testing the Web UI exposes the interval per virtual sensor. The initial defaults are deliberately conservative and will be tuned from real sensor captures:

- THGR810: 48 s
- UVN800: 60 s

The scheduler supports multiple virtual sensors from one physical ESP32. Frames are serialized; they are never intentionally transmitted simultaneously.

## Validation requirement

A profile is moved from `bench` to `validated` only after all of these pass:

1. logic-analyzer timing check at the transmitter DATA pin;
2. companion LILYGO raw-frame decode and checksum success;
3. correct sensor model / channel / rolling ID on the gateway;
4. correct engineering value on the gateway;
5. original WMR88/WMR200 console pairing and stable display;
6. repeated operation for at least 24 hours without sensor lockout or re-pairing.
