# LC29H_GNSS examples

Each folder is a complete Arduino sketch. Open the `.ino`, edit that folder’s `lc29hconfig.h` (where present), and flash.

Sketches stay smaller than a full NTRIP/Wi-Fi app. They show how to talk to the module:

- **Base:** 1 Hz RTCM (MSM7 + 1005) is the mission stream. NMEA is status.
- **Rover:** RTCM in is the mission stream. GGA (position) and RMC (time) go out every epoch for GIS tools. Other NMEA is slower.
- **Survey-in on LC29H(DA):** AccLimit 15 m. If the module already has a matching survey-in running, the sketch **adopts** it and does **not** send PAIR023 (that would reset `<Obs>`). A new survey is SAVEPAR then PAIR023, not PAIR003 sleep.
- **UART:** one reader, pump or ingest every `loop()`. Do not parse or write flash inside the pump callback.

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
| [ReducedSerialBridge](ReducedSerialBridge/) | Tiny SRAM raw USB↔GNSS |

Each folder has its own README that matches the comments at the top of the sketch. What the PQTM/PAIR/NMEA/RTCM payloads actually do (the part the Quectel PDF leaves thin) is in the library Readme: **[Module messages in practice](../Readme.md#module-messages-in-practice)**.
