# ISSUE3 B1 — BS 0.2.108 restore + expected-vs-actual rates

Date: 2026-11-05 (America/Chicago)

## Restore
- Method: `esptool` via **COM9 (CH340)** @ 460800 (COM12 CDC aborted mid-write repeatedly).
- Images from `%TEMP%\bs108_pre_lc29h_i3_20261105_141844`:
  - `bootloader_0108.bin` @ 0x0
  - `partitions_0108.bin` @ 0x8000
  - `boot_app0.bin` @ 0xe000
  - `nvs.bin` (live dump from running 0.2.108) @ 0x9000
  - `BaseStation.ino_flashed_0108.bin` @ 0x10000
- **Do not** flash stale `BaseStation.ino.bin` (Sept build = 0.2.86).
- Post-restore: `fw=0.2.108`, `gnssState=fixed`, `pauseReasons=0`, heap ~164k @ `http://192.168.2.91`.

## Reconfigure
`POST /api/gnss/reconfigure` → `{"ok":true}` (Fixed profile).

## Expected vs actual (Fixed / `kEntriesFixed`)

| Msg | Expected RATE | Actual | Live 25s Δrx | Verdict |
|-----|---------------|--------|--------------|---------|
| RMC | 1 | 1 | >0 | PASS live |
| GSA | 8 | 8 | >0 | PASS live |
| GGA | 0 | 0 | 0 | Expected mute — **waiver candidate** |
| GST | 0 | 0 | 0 | Expected mute (+ overnight gstN=0) — **waiver candidate** |
| GSV | 0 | 0 | 0 | Expected off; API needs Char+NTRIP pause — **waiver candidate** |
| PQTMSVINSTATUS | 0 | 0 | 0 | Survey-only mute — **waiver candidate** |
| PQTMEPE | 0 | 0 | 0 | Expected mute |

Source: Base Station `GnssMessageSchedule.h` Fixed profile. Full JSON: `%TEMP%\lc29h_i3_hil\b1_rates\B1_expected_vs_actual.json` (also copied under `/workspace/lc29h-i3-hil/b1_rates/`).

## Waiver list (for Brian)
1. Fixed: GGA rate=0 (schedule mute)
2. Fixed: GST rate=0 (schedule mute; overnight soak gstN=0 on EA)
3. Fixed: PQTMSVINSTATUS rate=0 (survey-only)
4. Fixed: GSV rate=0 until Char + NTRIP-pause gated `/api/gnss/gsv`

These zeros are intentional Fixed NMEA diet (RTCM-priority), not failed restore.
