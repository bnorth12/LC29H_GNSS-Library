# ReducedCommandConsole

For Uno/Micro/Feather SRAM. There is **no** `LC29H_GNSS` object. The sketch builds checksums and sends payloads you type.

## What it does

- USB Serial commands: `help`, `status`, `rover`, `base`, `fixrate`, `save` (PQTMSAVEPAR), `reboot` (PAIR023), hot/warm/cold, `send <payload>`.
- Every loop, GNSS UART bytes are copied to USB Serial.
- Survey-in on DA: `send PQTMCFGSVIN,W,1,3600,15,0,0,0` then `save` then `reboot`. AccLimit 15 m. PAIR003 is not a reboot.
- Rover GIS: keep GGA/RMC enabled; use `send PQTMCFGMSGRATE,W,GSV,10` (1 Hz fix) so GSV does not fill the UART.

## Hardware

Serial1 when present; otherwise SoftwareSerial RX4/TX3.
