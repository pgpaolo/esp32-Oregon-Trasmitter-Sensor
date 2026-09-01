# Hardware validation plan

Project status: **BETA / TESTING**.

The purpose of this plan is to avoid debugging RF timing, sensor encoding and original-console behavior at the same time.

## Stage 0 - firmware / UI

- build `esp32dev` and `esp32c3` in CI;
- boot without BME280 and confirm the Web UI still starts;
- connect BME280 at `0x76` and `0x77` and verify auto-detection;
- save configuration and reboot to confirm NVS persistence;
- verify OTA upload page;
- verify rolling IDs do not change after reset.

## Stage 1 - baseband only

Hardware: ESP32 + logic analyzer, RF transmitter disconnected.

1. Set THGR810 to a fixed calibration result, e.g. 21.2 C / 54 %.
2. Press `Transmit now`.
3. Capture RF DATA GPIO.
4. Check nominal half-bit around 488 us.
5. Check 24-bit preamble and sync sequence.
6. Confirm repeated packets and repeat gap.
7. Toggle `Invert RF polarity` and verify waveform inversion only.

Record captures in `docs/captures/` when testing starts.

## Stage 2 - companion LILYGO receiver

Hardware: ESP32 TX + FS1000A + existing LILYGO/SX1278 gateway.

Expected THGR810 values:

- sensor code `F824`;
- configured channel;
- configured rolling ID;
- valid checksum;
- calibrated temperature;
- calibrated humidity.

Expected UVN800 values:

- sensor code `D874`;
- configured rolling ID;
- valid checksum;
- integer UV index matching the TX preview.

If the LILYGO sees RF bursts but no valid frame, retain the raw edge/timing diagnostics before changing the transmitter.

## Stage 3 - WMR200

Start with only one virtual profile enabled.

### THGR810

1. Select an unused channel.
2. Set a known rolling ID.
3. Reset/search the WMR200 sensor reception only if needed.
4. Transmit at the configured cadence for at least 10 minutes.
5. Confirm stable temperature and humidity.
6. Reboot ESP32 and verify the console continues to identify the same sensor.

### UVN800

Repeat with THGR disabled to avoid diagnostic ambiguity. Confirm UV display and loss/recovery behavior.

## Stage 4 - WMR88

Repeat Stage 3. Pay particular attention to the maximum number of simultaneously accepted remote sensors and channel collisions.

## Stage 5 - multi-sensor scheduler

Enable THGR810 + UVN800 together.

Verify:

- no overlapping transmissions;
- stable reception on LILYGO;
- stable reception on WMR200/WMR88;
- no ID collisions;
- no console lockout after 24 hours.

## Stage 6 - PCR800 and WGR800

These encoders are currently **bench testing only**.

Before enabling them on an original console:

- compare encoder output against genuine captured frames;
- verify units and BCD digit order;
- verify checksum position;
- verify cadence;
- validate low-battery/status bits;
- verify wind direction index mapping against original WGR800 captures.

## Acceptance criteria for leaving beta

A release candidate requires:

- CI green for both target boards;
- THGR810 validated on LILYGO + WMR200 + WMR88;
- UVN800 validated on LILYGO + at least one original console;
- 72-hour mixed-profile soak test;
- no spontaneous ID changes;
- documented measured RF timing;
- documented tested FS1000A supply and range;
- at least one captured raw frame per validated profile stored in the repository.
