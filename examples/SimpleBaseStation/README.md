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

- Mega-class AVR, or ESP32 Serial1 (RX16/TX17). Uno/Nano run out of RAM. On Mega the Serial `help` command loop is compiled out so the sketch fits in 8 kB SRAM.
- GNSS UART 115200 8N1.

## Config

Edit `lc29hconfig.h` in this folder. `LC29H_CFG_SURVEY_MIN_TIME_SEC` is MinDur (fix count at 1 Hz). Leave `LC29H_CFG_FINALIZE_SURVEY_TO_FIXED` at 0 while survey-in is running.

## Messages this sketch uses

Full field notes: [Module messages in practice](../../Readme.md#module-messages-in-practice).

**To the module**

- `PQTMCFGSVIN,R` — if Mode=1 and MinDur/AccLimit match, adopt (no PAIR023).
- Otherwise `PQTMCFGRCVRMODE,W,2` + `PQTMCFGSVIN,W,1,<MinDur>,15,0,0,0` + `PQTMSAVEPAR` + `PAIR023`.
- `PAIR432,1` and `PAIR434,1` — MSM7 + 1005 at 1 Hz (produced even though this sketch discards RTCM).
- `PQTMCFGMSGRATE` for RMC/GGA RATE 1, GST/GSA/PQTMEPE RATE 5, GSV and `PQTMSVINSTATUS` RATE 10.

**From the module**

- `$PQTMSVINSTATUS` — Valid 1 and Obs counting; MeanAcc `0.0000` is not complete.
- GGA/RMC (status), GST/GSA/GSV (slow status). RTCM is parsed only so NMEA can be split out.
