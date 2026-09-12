# RoverCorrectionBridge

Bench **rover** half of a wired pair. Highest priorities: **ingest RTCM** from the link UART into the GNSS, and **publish GGA (position) and RMC (time)** for GIS. GST is also forwarded for accuracy; GSV is slowed to about 10 s.

## What it does

1. `LC29H_bringUp()` identifies the IC (`PQTMVERNO`), applies rover profile, SAVEPAR, PAIR023 if needed. Swap later with `module_reinit rover`.
2. ESP32: RTCM in is CRC-assembled (`LC29H_Rtcm`), then `LC29H_HostPump` drains GNSS NMEA. Do not `while (readLine)`.
3. AVR: `ingestRawAvailable` first, then a **capped** `readLine` count.

## Hardware

Pair with BaseSerialBridge. ESP32 GNSS Serial1 RX16/TX17, correction Serial2 RX18/TX19. Mega-class AVR uses SoftwareSerial (listen-switch between GNSS and link).

## Config

`LC29H_CFG_ROVER_PRINT_LOCAL_NMEA` and `LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK` in `lc29hconfig.h`. `LC29H_CFG_FIX_RATE_MS` sets the rover epoch.

## Messages this sketch uses

Same **to GNSS** rover GIS path as SimpleRover (`PQTMCFGRCVRMODE,W,1`, fix rate, GGA/RMC RATE 1, GSV ~10 s). [Module messages in practice](../../README.md#module-messages-in-practice).

**Into GNSS:** raw RTCM from the link UART first every loop.  
**From GNSS:** GGA (position) and RMC (time) for GIS; GST if you forward the allowlist.
