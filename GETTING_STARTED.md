# Getting started (adopters)

This library configures Quectel **LC29H** modules (BA / BS / DA / EA) over UART and keeps mixed **NMEA + RTCM** from overflowing the MCU. It is **not** an NTRIP caster, not SW Maps, and not a complete GIS app.

If you only read one file besides an example README, read this.

## 1. Pick an example (do not start from a blank sketch)

| You have | Open this |
| --- | --- |
| ESP32 + LC29H as **survey base**, RTCM out a second UART | `examples/ESP32BaseStation` or `SimpleBaseStation` |
| ESP32 + LC29H as **rover**, RTCM in from UART | `examples/SimpleRover` or `RoverCorrectionBridge` |
| ESP32-S3 + phone (SW Maps) over **BLE** | `examples/ESP32BtRoamer` |
| Dual USB-C + **QGNSS** on the CH343 port | `examples/ESP32UsbUartBridge` |
| Bench pair (two UARTs, no Bluetooth) | `BaseSerialBridge` + `RoverCorrectionBridge` |
| Mega 2560, tiny SRAM | `ReducedCommandConsole` / `ReducedSerialBridge` (no 8 kB pump) |
| Learn commands | `BasicConfiguration` then type `help` |

Copy that folder, edit **its** `lc29hconfig.h` (pins, role). Do not copy `lc29hconfig.h.template` into the library root.

## 2. What every sketch does at boot

1. Open the GNSS UART (`LC29H_beginEsp32GnssUart` on ESP32: **set RX buffer before `begin`**, default 8192 bytes).
2. **Identify** the IC: `$PQTMVERNO` → `Module family=DA|EA|BA|BS`. Wrong family ⇒ wrong commands.
3. Apply the **role** (rover or base). DA/EA: `SAVEPAR` then **`PAIR023`** (full reboot). `PAIR003` sleep is not a reboot. BS does not use PAIR023.
4. ESP32: **one UART reader**. Every `loop()`: `pump.drain` then `pump.frame`, then `processRtcm` / `processNmea`. Never `while (readLine)` until the port is empty.

Serial console (sketches that call `processSerialCommands`):

```text
module_ident
module_reinit rover
module_reinit base
help
```

Use `module_reinit` after you **swap** an LC29H or change role. Wait ~3 s after reboot, then `module_ident`.

## 3. Headers (what to `#include`)

| Header | When |
| --- | --- |
| `LC29H_GNSS.h` | Send PQTM/PAIR, checksums, rover/base helpers |
| `LC29H_ProjectConfig.h` | `lc29hconfig.h` + `LC29H_bringUp` / `LC29H_roverFactoryBringUp` |
| `LC29H_UartPump.h` | ESP32 FIFO drain/frame (required if NMEA+RTCM share one UART) |
| `LC29H_HostPump.h` | Thin `beginGnss` / `tick` / `processTo` used by examples |
| `LC29H_Rtcm.h` | CRC-24Q assemble BLE/UART RTCM **before** `writeRaw` |
| `LC29H_NmeaCompat.h` | `$PQTMEPE` → `$GNGST`; parse family from VERNO |
| `LC29H_ModuleSetup.h` | `LC29H_identifyModule`, family policy |
| `LC29H_MessageSchedule.h` | Base vs rover vs phone NMEA rate tables |

AVR: do **not** include `LC29H_UartPump.h` (it `#error`s). Use `forwardBridgeAvailable` with the 15 ms cap.

## 4. Success looks like

**Base:** `$PQTMSVINSTATUS` `Valid=1` (or 2), Obs counting, RTCM 1005 + MSM4/MSM7 leaving the RTCM UART. MeanAcc `0.0000` is a placeholder, not “survey complete.”

**Rover UART:** RTCM bytes in **before** NMEA print. GGA quality `1` standalone, `2` DGPS/SBAS, `5` RTK float, `4` RTK fixed.

**ESP32BtRoamer:** amber pulse (config) → blue pulse (GGA, connect BLE) → green (connected) + blue TX + red RX. CH343 log: `t1005` and `msm7` climbing, `rtcmCrcFail=0`.

## 5. Pitfalls we hit in the field (read these)

1. **Unbounded `readLine()`** on ESP32. The LC29H can emit GSV + MSM faster than you print. UART FIFO overflows; RTCM is lost; BLE looks “connected with no data.”
2. **`VERIFY=1` after rover writes.** PQTM readback is starved by NMEA. You get `VerifyFailed` even when the writes worked. Prefer `VERIFY=0` on a live DA, or drain with the pump before queries.
3. **SAVEPAR without PAIR023 on DA/EA.** Rover/base mode and survey-in do not take effect. Fitness nav (`PAIR081,1`) can leave a DA in DGPS/float; use **normal** (`PAIR081,0`).
4. **BLE MTU 23.** A GGA is ~80 bytes. SW Maps Generic NMEA does **not** glue 20-byte notifies. Request MTU ~185–517 and send **one complete sentence** per notify.
5. **USB CDC `Serial.println` on ESP32-S3.** If no host is reading native USB, prints can block the sketch (LED stuck, BLE dead). Use `Serial.setTxTimeoutMs(0)` and put debug on UART0/CH343.
6. **LC29H(DA) NMEA vs SW Maps.** DA often has **no GST**; GGA **DiffAge** and **station ID** are empty. Accuracy for Generic NMEA comes from synthesizing GST from `$PQTMEPE`. HDOP/PDOP/VDOP need **GSA**. Age/baseline/station ID will not appear.
7. **DGPS at ~5 ft with 1005+MSM7 on the wire.** The radio path is fine. The engine is not using RTCM as RTK (wrong mode, Fitness nav, or 1005 from an unfinished survey). Red LED on BtRoamer means BLE bytes, not “the module accepted RTK.”
8. **One GNSS UART.** Do not run QGNSS and a sketch pump on the same TX/RX. `ESP32UsbUartBridge` is the exception: it *is* the QGNSS pipe.

## 6. What this library will not do for you

- Host NTRIP (use SNIP, RTKLIB, or RTK2GO on a PC)
- Pair Windows Bluetooth Settings with BLE NUS (use SW Maps / nRF Connect)
- Guarantee RTK fixed; that is sky + base 1005 + MSM + rover mode
- Replace Quectel’s protocol/hardware PDFs or a general RTK textbook

## 7. External documentation (required)

Anyone shipping this module still needs the **Quectel ICDs** and a basic **RTK/RTCM** picture. This library only records how those commands are sent on Arduino and what failed in the field. We **cannot redistribute** Quectel PDFs.

### Quectel (create an account)

Quectel’s **Download Zone** is the official source. Register, then search **LC29H**. The product page lists the same files once you are signed in.

| Resource | URL |
| --- | --- |
| Download Zone (login) | https://www.quectel.com/download-zone |
| LC29H product / document list | https://www.quectel.com/product/gnss-lc29h/ |
| How to get documents (forum) | https://forums.quectel.com/t/how-to-get-quectel-documents/30162 |
| GNSS forum | https://forums.quectel.com/c/gnss-module/ |
| Technical support | https://www.quectel.com/tech-support/ |
| QGNSS tool (eval / firmware) | search **QGNSS** in Download Zone |

Documents this library was written against (get the **latest** from Download Zone; titles stay similar across versions):

| Document | Why you need it |
| --- | --- |
| **LC29H & LC79H Series GNSS Protocol Specification** (v1.5 used here) | PQTM/PAIR, NMEA, which commands exist on DA/EA/BA |
| **LC29H(BS) GNSS Protocol Specification** (v1.1 used here) | Separate ICD; no PAIR023 |
| **LC29H(BA,CA,DA,EA) DR & RTK Application Note** | Rover vs base, RTCM in/out, survey-in |
| **LC29H Series Hardware Design** | Pins, 3.3 V, UART1 vs UART2, antenna |
| **LC29H Series GNSS Specification** (datasheet) | Which variant is RTK 1 Hz (DA) vs 10 Hz (EA) vs DR |
| **LC29H Series Reference Design** | Schematic checklist |
| **Firmware upgrade guide** + QGNSS user guide | If you must change module firmware (GST/PQTMEPE often need a newer build) |

Firmware is usually **not** on the public product page. Quectel support or your distributor sends it after you quote `$PQTMVERNO`.

### RTK / NMEA / RTCM (not Quectel)

| Topic | Starting points |
| --- | --- |
| What RTK, float, fixed, 1005, MSM mean | NOAA/NGS CORS and “Introduction to GNSS RTK” notes; [rtklibexplorer](https://rtklibexplorer.wordpress.com/) (practical RTCM3 / MSM) |
| RTCM 3 message types | [RTCM](https://www.rtcm.org/) standard **10403.x** (paid). Informal type lists: 1005/1006 ARP, 1074/1077 GPS MSM4/7, 108x GLO, 109x GAL, 112x BDS |
| NMEA 0183 | [NMEA](https://www.nmea.org/) (GGA quality 1/2/4/5, GST, GSA). SW Maps Generic NMEA follows this, not `$PQTMEPE` |
| NTRIP caster | [RTK2GO how to connect](http://rtk2go.com/how-to-connect/) (port 2101, case-sensitive mountpoint, email as user) |
| Open-source rover/base tools | [RTKLIB](https://github.com/tomojitakasu/RTKLIB) / demo5 forks |

GGA quality cheat sheet used by this library’s examples: **1** standalone, **2** DGPS/SBAS, **5** RTK float, **4** RTK fixed. Quality 2 at ~1.5 m with 1005+MSM7 on the UART often means the **engine** is not in rover/normal nav, not that BLE dropped the stream.

## 8. After you have one example working

Read **[Module messages in practice](README.md#module-messages-in-practice)** for RATE vs Hz, AccLimit, and which sketch sends which payload. Then copy the example’s `loop()` order: **corrections first, NMEA second, no flash writes in the pump callback.**
