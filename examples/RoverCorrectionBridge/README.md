# RoverCorrectionBridge

Bench **rover** half of a wired pair. Highest priorities: **ingest RTCM** from the link UART into the GNSS, and **publish GGA (position) and RMC (time)** for GIS. GST is also forwarded for accuracy; GSV is slowed to about 10 s.

## What it does

1. `LC29H_bringUp()` applies the rover profile, then GIS NMEA rates (GGA/RMC/VTG every epoch).
2. Every loop, `ingestRawAvailable()` **first** so corrections are not starved.
3. Then NMEA is printed locally and/or forwarded on the link, using the allowlist (GGA, RMC, GST on).

## Hardware

Pair with BaseSerialBridge. ESP32 GNSS Serial1 RX16/TX17, correction Serial2 RX18/TX19. Mega-class AVR uses SoftwareSerial (listen-switch between GNSS and link).

## Config

`LC29H_CFG_ROVER_PRINT_LOCAL_NMEA` and `LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK` in `lc29hconfig.h`. `LC29H_CFG_FIX_RATE_MS` sets the rover epoch.
