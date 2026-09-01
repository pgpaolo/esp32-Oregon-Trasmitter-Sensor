# ESP32 Oregon Transmitter Sensor

> **Status: BETA / TESTING - not yet validated against WMR88/WMR200 hardware.**

ESP32-based multi-sensor emulator for Oregon Scientific 433.92 MHz sensors. Physical sensors connected to the ESP32 are calibrated, mapped to virtual Oregon Scientific devices, encoded as genuine Oregon-compatible OSV3 frames and transmitted by a low-cost OOK/ASK 433 MHz TX module.

The same RF frames are intended to be received by original Oregon Scientific consoles (WMR88/WMR200) and by the companion `esp32-oregon-technoline-weather-gateway` project. There is intentionally **no private RF protocol path**: the gateway receives the node exactly as an Oregon receiver would.

## Current beta scope

- ESP32 / ESP32-C3 architecture (PlatformIO / Arduino framework)
- FS1000A-compatible OOK/ASK transmitter on configurable GPIO
- BME280 input: temperature, humidity and pressure (pressure retained locally unless mapped to a compatible Oregon profile)
- Analog UV input as a generic first-stage source; architecture prepared for digital UV drivers
- Virtual sensor configuration stored in NVS
- Web setup console with calibration offsets, channel, rolling ID, TX interval, repeat count and model selection
- Browser firmware update endpoint
- OSV3 encoder/transmitter framework
- Experimental profiles:
  - THGR810 / THGN801 (`F824`) - temperature/humidity
  - UVN800 (`D874`) - UV index
  - PCR800 (`2914`) - rain, encoder implemented for bench validation
  - WGR800 (`1984`) - wind, encoder implemented for bench validation
- Serial diagnostics and dry-run frame dump

## Important beta warning

The software is deliberately marked **BETA / TESTING**. The frame builders follow the Oregon V3 layout used by the companion gateway, but RF timing, packet cadence and console acceptance must be validated on real WMR88/WMR200 hardware before this firmware is considered production-ready.

## Architecture

```text
BME280 / UV / rain / wind inputs
              |
              v
      calibration + mapping
              |
              v
     Virtual Oregon Sensors
   THGR810 / UVN800 / PCR800 / WGR800
              |
              v
         OSV3 encoder
              |
              v
       GPIO OOK waveform
              |
              v
      FS1000A 433.92 MHz
         /            \
        v              v
 WMR88 / WMR200   ESP32 LILYGO gateway
                     (Oregon RX)
```

## Recommended prototype hardware

- ESP32 DevKit or ESP32-C3 board
- FS1000A or equivalent 433 MHz ASK/OOK transmitter
- BME280 (I2C)
- UV sensor (initial beta: analog input; digital drivers can be added)
- 17.3 cm quarter-wave wire antenna as a starting point

### Default pins

| Function | ESP32 default | Notes |
|---|---:|---|
| RF DATA | GPIO 25 | configurable at compile time |
| BME280 SDA | GPIO 21 | configurable |
| BME280 SCL | GPIO 22 | configurable |
| UV analog | GPIO 34 | ESP32 only; select a valid ADC pin on C3 |
| Status LED | GPIO 2 | optional |

The RF transmitter may be powered according to its module specification. **Always share GND with the ESP32.** Verify that the transmitter DATA input accepts 3.3 V logic before use.

## Web console

At first boot the firmware starts an AP:

```text
SSID: Oregon-TX-Setup
URL : http://192.168.4.1/
```

The console exposes:

- node name and Wi-Fi STA credentials
- RF TX GPIO and master enable
- virtual Oregon sensor model
- Oregon channel
- rolling ID
- low-battery flag
- source mapping
- calibration offset / multiplier
- transmission interval
- repeated frames and inter-repeat gap
- live source values
- generated raw frame preview
- manual `Transmit now`
- OTA firmware upload (`/update`)

Configuration is stored in ESP32 NVS/Preferences.

## Oregon identity model

Three fields are intentionally separate:

- **Sensor code**: fixed by the emulated Oregon model, e.g. `F824` for THGR810.
- **Channel**: encoded using Oregon channel values (`1`, `2`, `4` for channels 1-3 where applicable).
- **Rolling ID**: configurable 8-bit identity of the virtual sensor and persisted across resets.

The firmware does **not** regenerate the rolling ID at every boot. This avoids unnecessary re-pairing of original consoles during development. A new ID can be generated explicitly from the Web UI.

## Data flow and calibration

Calibration is applied before Oregon encoding:

```text
physical reading -> offset/multiplier -> range clamp -> Oregon payload -> checksum -> RF
```

Example:

```text
BME280 21.63 C -> temperature offset -0.40 -> 21.23 C -> encoded 21.2 C
```

## Build

```bash
pio run
pio run -e esp32dev -t upload
pio device monitor
```

For ESP32-C3:

```bash
pio run -e esp32c3
```

## Repository layout

```text
include/             configuration and public headers
src/                 firmware implementation
docs/                protocol notes, wiring, test plan and PDF manual
.github/workflows/    CI build
platformio.ini        PlatformIO environments
```

## Validation plan

The first hardware milestone is intentionally narrow:

1. ESP32 + FS1000A transmits a fixed THGR810 frame.
2. Companion LILYGO gateway decodes sensor code, channel, rolling ID, temperature, humidity and checksum.
3. WMR200 receives and displays the same virtual THGR810.
4. Repeat with UVN800.
5. Measure RF timings with logic analyzer/oscilloscope and tune `OSV3_HALF_US` if required.
6. Validate cadence and long-running stability.
7. Then enable PCR800 and WGR800 console testing.

## Companion receiver

This project is designed to be tested with:

`pgpaolo/esp32-oregon-technoline-weather-gateway`

The receiver must see these transmissions as ordinary Oregon frames. No special GP/private packet format is required.

## License

GNU LGPLv3. See `LICENSE`.

Third-party protocol references and prior art are listed in `docs/REFERENCES.md`. The project code is an independent implementation intended for interoperability and laboratory testing.
