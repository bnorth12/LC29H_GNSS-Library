# ESP32BtRoamer

Phone-app rover. Highest priorities: **ingest RTCM/RTIP from the phone** and **publish GGA (position) and RMC (time)** back to the app for GIS.

## What it does

- Classic ESP32: Bluetooth SPP. ESP32-S3: BLE Nordic UART (NUS) RX/TX characteristics.
- `LC29H_bringUp()` sets rover GIS NMEA rates. GSV about every 10 s.
- Every loop: Bluetooth bytes → GNSS (`ingestRawAvailable`) **first**, then NMEA lines to USB Serial and/or the phone.

## Hardware

GNSS on Serial2, pins from `lc29hconfig.h` (default RX16/TX17). Pair the phone to `LC29H_CFG_ESP32_BT_NAME`.

## BLE (ESP32-S3)

- Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX (phone → rover corrections) `6E400002-...`
- TX (rover → phone NMEA) `6E400003-...`

SW Maps and similar tools must support this NUS write/notify model.
