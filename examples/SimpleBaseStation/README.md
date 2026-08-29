# SimpleBaseStation

Smallest survey-base sketch. It configures the LC29H as a survey-in base and drains mixed NMEA + RTCM on the GNSS UART. It is not a Wi-Fi or NTRIP application.

## What it does

1. Reads `lc29hconfig.h` (survey-base role, AccLimit 15 m, finalize off).
2. Calls `LC29H_bringUp()`:
   - If the module is already in survey-in with the same MinDur and AccLimit, **adopt** it. No CFGSVIN, no PAIR023, so `<Obs>` keeps counting.
   - Otherwise write base mode + CFGSVIN, save, and **PAIR023** (full module reboot). PAIR003/PAIR002 GNSS sleep is not a reboot.
3. Applies the base status NMEA schedule (GGA/RMC 1 s, GST/GSA/EPE 5 s, GSV and `$PQTMSVINSTATUS` 10 s). RTCM MSM7+1005 stay 1 Hz.
4. Every loop, `forwardBridgeAvailable()` drains the UART. RTCM is discarded (this sketch has no second port). Complete NMEA lines are printed. `$PQTMSVINSTATUS` Valid=1 and Obs counting means survey-in is running. MeanAcc `0.0000` is a placeholder until Obs > 0, not “complete”. Use `survey_finalize` only after Valid=2.

## Hardware

- Mega-class AVR, or ESP32 Serial1 (RX16/TX17). Uno/Nano run out of RAM.
- GNSS UART 115200 8N1.

## Config

Edit `lc29hconfig.h` in this folder. `LC29H_CFG_SURVEY_MIN_TIME_SEC` is MinDur (fix count at 1 Hz). Leave `LC29H_CFG_FINALIZE_SURVEY_TO_FIXED` at 0 while survey-in is running.
