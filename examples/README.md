# LC29H_GNSS examples

## Board pins and HIL notes (issue #3)

- Default ESP32 GNSS UART pins come from `lc29h_board.h`: **RX=17, TX=18** (Base Station / N16R8 HIL). Override with `-DLC29H_BOARD_GNSS_RX_PIN=…` / `TX`.
- ESP32-S3 Arduino: **Tools → USB CDC On Boot = CDC** so native USB (COM12) is the console.
- Start the UART pump / `setTimeout(0)` **after** bring-up so PQTM replies can match under live NMEA.
- Drain RTCM every loop with `LC29H_HostPump::processTo` (pass a discard `Stream` if you do not forward).
- Examples with `LC29H_CFG_SAVE=1` call **SAVEPAR** and leave the module non-default — restore the base-station message-rate set afterward (esp32s3-lc29h-base-station#12).
- Status lines: `mailboxOverwrites` = pump mailbox pressure; `usbCdcBackpressureDrops` = USB CDC could not accept a write (not GNSS UART loss).

**Start with [GETTING_STARTED.md](../GETTING_STARTED.md)** if you have not used this library. It says which example to open, what boot must do, field pitfalls, and **Quectel Download Zone / RTK links** (you still need those PDFs).

Each folder is a complete Arduino sketch. Open the `.ino`, edit that folderâ€™s `lc29hconfig.h` (where present), and flash.

Sketches stay smaller than a full NTRIP/Wi-Fi app. They show how to talk to the module:

- **Base:** 1 Hz RTCM (MSM7 + 1005) is the mission stream. NMEA is status.
- **Rover:** RTCM in is the mission stream. GGA (position) and RMC (time) go out every epoch for GIS tools. Other NMEA is slower.
- **Survey-in on LC29H(DA):** AccLimit 15 m. If the module already has a matching survey-in running, the sketch **adopts** it and does **not** send PAIR023 (that would reset `<Obs>`). A new survey is SAVEPAR then PAIR023, not PAIR003 sleep.
- **UART:** one reader every `loop()`. ESP32 sketches use `LC29H_UartPump` / `LC29H_HostPump` (drain then frame). Do not use unbounded `readLine()` on ESP32 â€” it starves RTCM and overflows the driver FIFO.
- **Identify first:** every bring-up sends `PQTMVERNO` and logs `Module family=DA|EA|BA|BS`. Swap modules with Serial `module_reinit rover` or `module_reinit base`.
- **DA rover boot:** restore â†’ rover mode â†’ `PAIR081,0` â†’ rates â†’ SAVEPAR â†’ PAIR023, with UART drain pauses (`LC29H_roverFactoryBringUp`). Do not verify PQTM reads against a live NMEA flood.
- **Phone GIS:** GGA/RMC/GSA + `$PQTMEPE` (host may synthesize GST). DA does not fill GGA DiffAge/station ID.

| Example | Role |
| --- | --- |
| [SimpleBaseStation](SimpleBaseStation/) | Smallest survey-base bring-up |
| [SimpleRover](SimpleRover/) | Smallest rover: RTCM in (ESP32), GGA/RMC out |
| [BasicConfiguration](BasicConfiguration/) | Interactive console on top of the same bring-up |
| [ESP32BaseStation](ESP32BaseStation/) | ESP32 base, RTCM out Serial2 |
| [StreamBridge](StreamBridge/) | Forward GNSS to an upstream UART |
| [BaseSerialBridge](BaseSerialBridge/) | Bench base half of a wired pair |
| [RoverCorrectionBridge](RoverCorrectionBridge/) | Bench rover half: RTCM in, GGA/RMC out |
| [ESP32BtRoamer](ESP32BtRoamer/) | Phone Bluetooth rover |
| [ESP32UsbUartBridge](ESP32UsbUartBridge/) | QGNSS passthrough on dual USB-C |
| [ReducedCommandConsole](ReducedCommandConsole/) | Tiny SRAM typed payloads |
| [ReducedSerialBridge](ReducedSerialBridge/) | Tiny SRAM raw USBâ†”GNSS |

Each folder has its own README that matches the comments at the top of the sketch. What the PQTM/PAIR/NMEA/RTCM payloads actually do (the part the Quectel PDF leaves thin) is in the library README: **[Module messages in practice](../README.md#module-messages-in-practice)**.
