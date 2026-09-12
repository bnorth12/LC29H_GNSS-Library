# SimpleRover

Smallest rover. **RTCM in first**, then NMEA out. ESP32 uses `LC29H_UartPump` (not unbounded `readLine`).

## Boot

`LC29H_bringUp()`:

1. `$PQTMVERNO` → logs `Module family=`
2. Family policy (DA: 1 Hz, `PAIR081,0`)
3. Rover mode, GIS or phone rates, SAVEPAR, PAIR023 if this IC needs it

Swap the module later: Serial `module_reinit rover`, wait 3 s, `module_ident`.

## Loop (ESP32)

1. `ingestRawAvailable` on the correction UART (or CRC-assemble if you use `LC29H_Rtcm`)
2. `uartPump.drain` / `frame` / `processNmea` with a millisecond budget
3. Do not print every GSV line on USB; it will overflow the GNSS RX FIFO

AVR: NMEA only; no pump. Use RoverCorrectionBridge for a second UART.

## Hardware

- ESP32: GNSS Serial1 RX16/TX17, corrections Serial2 RX5/TX4 (edit `lc29hconfig.h`)
- Mega: GNSS SoftwareSerial RX4/TX3. `help` is compiled out on Mega to fit SRAM.

## Messages

See [Module messages in practice](../../README.md#module-messages-in-practice) and [GETTING_STARTED.md](../../GETTING_STARTED.md).

DA often has **no GST**; GGA DiffAge/station ID stay empty. Phone GIS: GGA + RMC + GSA + `$PQTMEPE` (host may build GST).
