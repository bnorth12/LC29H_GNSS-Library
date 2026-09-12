# ESP32BtRoamer

Phone-app rover: ingest RTCM from SW Maps (NTRIP over BLE NUS), publish GGA/RMC/GST/GSA at 1 Hz.

This example is **not** a thin `readLine` loop. Field use showed that the original pattern fails on LC29H(DA) + ESP32-S3 BLE:

- Unbounded `readLine()` starves RTCM and overflows the UART FIFO
- `SAVEPAR` without `PAIR023` does not apply rover mode on DA
- Live verify against a NMEA flood returns VerifyFailed even when writes succeeded
- BLE default MTU 23 splits GGA; SW Maps does not reassemble
- DA has no GST / GGA DiffAge; SW Maps wants GST and GSA
- USB CDC `Serial.println` can block the S3 and freeze the RGB LED

## Boot (before BLE advertises)

Amber LED while this runs (UART drained between commands):

1. `PQTMRESTOREPAR`
2. `PQTMCFGRCVRMODE,W,1` (rover)
3. `PAIR081,0` (normal nav; Fitness has blocked DA RTK)
4. 1 Hz GGA/RMC/GSA/GST/PQTMEPE; GSV/VTG off
5. `PQTMSAVEPAR`
6. `PAIR023` + settle

Then wait for checksum-good GGA → **slow blue LED** → connect SW Maps.

## Loop

- `LC29H_UartPump` drain/frame (same FIFO idea as the base station)
- RTCM from BLE: CRC-24Q assemble, then `writeRaw` (`LC29H_Rtcm`)
- `$PQTMEPE` → synthetic `$GNGST` for SW Maps (`LC29H_NmeaCompat`)
- BLE TX: one complete NMEA sentence per notify, staggered 250 ms (GGA, RMC, GST, GSA)

## Hardware

ESP32-S3 N16R8 typical: GNSS Serial2 **RX=17 TX=18**, RGB GPIO 48.

Two USB-C ports: native USB for flash (do not open in a serial monitor). CH343 UART USB-C for 115200 debug (`CFG:` lines and 1 Hz status).

## BLE (ESP32-S3)

- Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX (phone → RTCM) `6E400002-...`
- TX (rover → NMEA) `6E400003-...`

Use SW Maps **Generic NMEA**, not Windows Add device. Connect only after the blue pulse.

## Status line (CH343)

`t1005` / `msm7` / `rtcmCrcFail=0` / `btBytesToGnss` equal bytes in means corrections reached the LC29H. DGPS vs float vs fixed is then the GNSS engine (and the base 1005 survey), not BLE.
