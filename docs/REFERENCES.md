# References and prior art

This project is an independent interoperability implementation. The following material is useful during validation and reverse-engineering work.

## Protocol references

- Oregon Scientific RF Protocols, version 2.1 / 3.0 protocol description and known sensor IDs: https://www.osengr.org/Articles/OS-RF-Protocols-IV.pdf
- rtl_433 Oregon Scientific decoder: https://github.com/merbanan/rtl_433/blob/master/src/devices/oregon_scientific.c
- Oregon_NR Arduino library by Sergey Zawislak (MIT), receive/transmit research and emulation support: https://github.com/invandy/Oregon_NR
- Joseph Shuhy OSv3 THGR810 emulator for WMR200, generic 433 MHz TX: https://www.shuhy.com/esi/osv3_dock_sensor.htm

## Companion project

- pgpaolo/esp32-oregon-technoline-weather-gateway

The transmitter frame representation is deliberately aligned with the companion gateway's current Oregon parser so TX/RX validation can be performed byte-for-byte before testing original consoles.

## Sensor IDs used in this beta

| Model | ID |
|---|---|
| THGR810 / THGN801 | `F824` |
| UVN800 | `D874` |
| PCR800 | `2914` |
| WGR800 | `1984` |

These identifiers are public protocol interoperability values and are not project-specific IDs.

## Licensing

The project itself is released under GNU LGPLv3. Third-party code is not copied into this repository unless its original license and attribution are explicitly retained. Protocol behavior has been reimplemented from public documentation and observed interoperability requirements.
