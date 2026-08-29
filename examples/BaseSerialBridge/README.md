# BaseSerialBridge

Bench **base** half of a wired pair. GNSS UART is parsed; RTCM is forwarded to a rover on a second UART. Pair with RoverCorrectionBridge.

## What it does

- `LC29H_bringUp()` as survey-base: adopt a matching live SVIN, or CFGSVIN + SAVEPAR + PAIR023.
- RTCM stays 1 Hz. Status NMEA is scheduled so GSV does not fill the UART.
- Every loop, `forwardBridgeAvailable()` to the rover link. Optional local NMEA print.

## Hardware

Same UART mapping as StreamBridge. Connect base link TX to rover link RX, and GND.

## Config

This folder’s `lc29hconfig.h`. Keep `LC29H_ROLE` as `LC29H_ROLE_BASE_SURVEY`.

## Messages this sketch uses

Same **to GNSS** survey-base path as SimpleBaseStation. [Module messages in practice](../../Readme.md#module-messages-in-practice).

**From GNSS:** RTCM MSM7+1005 **out the rover link** (that is the mission for the pair). Optional GGA locally.
