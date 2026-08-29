# ESP32BaseStation

ESP32-only survey-base. GNSS on Serial1, RTCM forwarded on Serial2. Not a Wi-Fi/NTRIP app — a hardware starting point.

## What it does

1. UART pins come from this folder’s `lc29hconfig.h` (defaults avoid ESP32-S3 native USB GPIOs 19/20).
2. `LC29H_bringUp()` adopts a matching live survey-in, or CFGSVIN AccLimit 15 m + SAVEPAR + PAIR023.
3. Base status NMEA schedule; RTCM MSM7+1005 stay 1 Hz (mission stream).
4. Every loop, `forwardBridgeAvailable()` pumps Serial1 to Serial2. Complete NMEA can be mirrored to USB Serial. Do not parse or write flash inside that callback.

## Hardware

ESP32-S3-DevKitC-1 class, dual USB-C. Default GNSS UART1 RX18/TX17, RTCM UART2 RX5/TX4.

## Config

Edit `lc29hconfig.h` for clone-specific GPIO mapping and survey MinDur.
