# ESP32UsbUartBridge

Fast bring-up when the ESP32-S3 board has two USB-C ports. Native USB is the Arduino console. The CH340 USB-C is Serial0, a raw GNSS UART for QGNSS.

## What it does

- GNSS module on Serial1 (default RX18/TX17).
- Loop: host bytes on Serial0 are written raw to GNSS; GNSS bytes are forwarded raw to Serial0.
- **One controller at a time:** QGNSS on CH340, or Arduino Serial commands, not both.
- Auto-apply is **off** by default so QGNSS owns baud, rates, and survey-in. Set `LC29H_CFG_ESP32_USB_UART_BRIDGE_APPLY_PROJECT_CONFIG` to 1 only if you want `LC29H_bringUp()` before the bridge.

## Hardware

ESP32-S3 dual USB-C. Keep GPIO44/43 free (CH340 U0RXD/U0TXD).

## Config

Toggles in this folder’s `lc29hconfig.h`: `APPLY_PROJECT_CONFIG`, `STRICT_OWNERSHIP`, `STARTUP_SANITY_CHECKS`, bridge baud.
