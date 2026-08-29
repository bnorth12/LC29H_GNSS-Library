# StreamBridge

Forwards GNSS output to an upstream UART (second board, radio, or host). RTCM is the mission stream; optional NMEA follows the allowlist in `lc29hconfig.h`.

## What it does

- `LC29H_bringUp()` for the configured role (default survey-base): adopt live SVIN or start it, then schedule status NMEA.
- Every loop, `forwardBridgeAvailable()` sends RTCM (and allowed NMEA) to the link UART.
- Pump every loop. Do not parse inside the NMEA callback.

## Hardware

- ESP32: GNSS Serial1 RX16/TX17, link Serial2 RX18/TX19 (GPIO19 may conflict with native USB on S3).
- Mega-class AVR: SoftwareSerial GNSS RX4/TX3, link RX6/TX5.

## Config

`LC29H_CFG_BRIDGE_MODE` and the NMEA allowlist in `lc29hconfig.h`.

## Messages this sketch uses

Same survey-base **to GNSS** commands as SimpleBaseStation. [Module messages in practice](../../Readme.md#module-messages-in-practice).

**From GNSS:** RTCM (always, if enabled) plus allowlisted NMEA (default GGA) **out the link UART**. GSV is RATE 10 so it does not fill that link.
