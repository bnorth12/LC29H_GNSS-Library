# ReducedSerialBridge

Smallest possible USB Serial ↔ GNSS UART byte bridge. No library object, no configuration.

## What it does

Every loop, copies GNSS → USB and USB → GNSS. One reader on each side. Do not also open QGNSS or another client on the same USB port.

Use this to talk to QGNSS or a host tool through the board, or to confirm wiring.

## Hardware

Serial1 when present; otherwise SoftwareSerial RX4/TX3. 115200 8N1.
