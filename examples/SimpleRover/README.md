# SimpleRover

Smallest rover sketch. Highest priorities: **accept RTCM corrections** and **publish GGA (position) and RMC (time)** every navigation epoch for a GIS mapping tool. GST/GSA/ZDA run about 1 Hz; GSV every 10 s.

## What it does

1. Applies the UAS rover profile from `lc29hconfig.h` (fix interval, default 200 ms).
2. `LC29H_bringUp()` then sets GIS NMEA rates and SAVEPAR. PAIR023 only if the profile needs a module reboot.
3. **ESP32:** Serial2 is the correction UART. Every loop it `ingestRawAvailable()` **first**, then prints NMEA. Do not let printing starve corrections.
4. **AVR:** NMEA only. Use RoverCorrectionBridge if you need a second UART for RTCM.

## Hardware

- ESP32: GNSS Serial1 RX16/TX17, corrections Serial2 RX5/TX4.
- Mega-class AVR: GNSS SoftwareSerial RX4/TX3.

## Config

`LC29H_CFG_FIX_RATE_MS` is the rover epoch. GGA/RMC/VTG use RATE 1 (every epoch). GSV RATE is chosen so the sentence is about every 10 s at that fix rate.
