# LC29H_GNSS

Configuration-focused Arduino library starter for Quectel LC29H modules.

## Current status

Initial library skeleton is now implemented with:

- GNSS transport on any Arduino Stream
- Automatic NMEA-style checksum generation
- Command send helpers for PQTM and PAIR payloads
- Receiver mode helpers
  - Rover: PQTMCFGRCVRMODE,W,1
  - Base: PQTMCFGRCVRMODE,W,2
- Rover output/fix rate helper: PQTMCFGFIXRATE,W/R
- Receiver mode query: PQTMCFGRCVRMODE,R
- Survey-In helper
- Message output control and query: PQTMCFGMSGRATE,W/R
- Parsed message-rate queries for app logic: getMessageRate(...)
- UART baud rate set/query: PQTMCFGUART,W/R
- Parsed baud query: getBaudRate(...)
- Fix rate set/query: PQTMCFGFIXRATE,W/R
- Parsed fix-rate query: getFixRateMs(...)
- Additional configuration wrappers: PQTMCFGCNST, PQTMCFGNAVMODE, PQTMCFGNMEADP, PQTMCFGNMEATID, PQTMCFGPROT
- Start/stop and reset helpers: PQTMGNSSSTART, PQTMGNSSSTOP, PQTMHOT, PQTMWARM, PQTMCOLD
- Base fixed-position helper (decimal degrees to ddmm.mmmm conversion; uses the observed log-backed fixed-base command path)
- Preset-level one-call rover/base workflows with optional save
- Mission profiles for UAS rover, survey base, and static base with post-config verification queries
- Survey-in ECEF capture and fixed ECEF write-back workflow
- Structured error model with last-error context and optional host callback events
- Serial Monitor command processor for rapid bench testing

This starter intentionally focuses on reliable command transport and reproducible setup flow first.

Raw-data design note:

- This library is intentionally compatible with higher-layer NMEA/RTK stacks.
- It can be used for configuration/control while raw GNSS/RTCM bytes are forwarded upstream.
- Parsing can remain in dedicated libraries/apps (for example TinyGPS++, RTK parsers, SW Maps feeders, NTRIP bridge apps).

## Known limits

Board-driven default sizing:

- Tracker enable remains opt-in through config macros.
- Point count defaults are selected by target board family when MAX_POINTS is set to 0.
- AVR default: 48 points (SRAM-safe baseline).
- ESP32 default: base 200 points, rover 120 points.
- Other boards default: base 120 points, rover 96 points.

Override behavior:

- Set LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE or LC29H_CFG_ROVER_ACCURACY_TRACK_ENABLE to 1 to enable tracking.
- Set LC29H_CFG_*_ACCURACY_TRACK_MAX_POINTS to a non-zero value to force an explicit point count.
- Keep LC29H_CFG_*_ACCURACY_TRACK_MAX_POINTS at 0 to use board-driven defaults.

## Folder layout

- LC29H_GNSS.h
- LC29H_GNSS.cpp
- LC29H_ProjectConfig.h
- lc29hconfig.h.template
- examples
  - BasicConfiguration/BasicConfiguration.ino
  - BasicConfiguration/lc29hconfig.h
  - SimpleRover/SimpleRover.ino
  - SimpleRover/lc29hconfig.h
  - SimpleBaseStation/SimpleBaseStation.ino
  - SimpleBaseStation/lc29hconfig.h
  - StreamBridge/StreamBridge.ino
  - StreamBridge/lc29hconfig.h
  - BaseSerialBridge/BaseSerialBridge.ino
  - BaseSerialBridge/lc29hconfig.h
  - ESP32BaseStation/ESP32BaseStation.ino
  - ESP32BaseStation/lc29hconfig.h
  - ESP32UsbUartBridge/ESP32UsbUartBridge.ino
  - ESP32UsbUartBridge/lc29hconfig.h
  - ESP32BtRoamer/ESP32BtRoamer.ino
  - ESP32BtRoamer/lc29hconfig.h
  - RoverCorrectionBridge/RoverCorrectionBridge.ino
  - RoverCorrectionBridge/lc29hconfig.h
  - ReducedCommandConsole/ReducedCommandConsole.ino
  - ReducedSerialBridge/ReducedSerialBridge.ino
- CommandReference.md

Note: this repository currently keeps both source files and metadata/docs in the root directory.

## Quick start

1. Copy/open this repository in your Arduino libraries folder.
2. Open the example-local lc29hconfig.h in the example directory you want to run.
3. Adjust role and values in that lc29hconfig.h for your project type (UAS rover, base survey, or static base).
4. Open examples/BasicConfiguration/BasicConfiguration.ino.
5. Set GNSS serial pins/port for your board.
6. Open Serial Monitor at 115200 baud.
7. Type help and use interactive commands.

Example config behavior:

- All examples include LC29H_ProjectConfig.h and require lc29hconfig.h.
- Each example directory includes its own lc29hconfig.h; edit that local file before running the sketch.
- The examples print a clear message and stay disabled until that file is provided.

## Minimal examples

- SimpleRover: examples/SimpleRover/SimpleRover.ino
  - Applies UAS rover profile with save + verify.
- SimpleBaseStation: examples/SimpleBaseStation/SimpleBaseStation.ino
  - Applies survey-base profile with RTCM + save + verify.
- BasicConfiguration: examples/BasicConfiguration/BasicConfiguration.ino
  - Full reference/demo flow including project config and survey ECEF capture.
- BaseSerialBridge: examples/BaseSerialBridge/BaseSerialBridge.ino
  - Bench base pipeline: GNSS output parsed and forwarded to rover over UART link.
- ESP32BaseStation: examples/ESP32BaseStation/ESP32BaseStation.ino
  - ESP32-only base station bring-up with Serial1/Serial2 and example-local UART GPIO mapping in lc29hconfig.h.
- ESP32UsbUartBridge: examples/ESP32UsbUartBridge/ESP32UsbUartBridge.ino
  - Native USB console plus CH340-backed second USB-C bridge for fast GNSS/QGNSS bring-up over Serial0.
- ESP32BtRoamer: examples/ESP32BtRoamer/ESP32BtRoamer.ino
  - ESP32 rover path that ingests RTCM corrections from a phone app over Bluetooth, using BT Classic SPP on classic ESP32 and BLE on ESP32-S3.
- RoverCorrectionBridge: examples/RoverCorrectionBridge/RoverCorrectionBridge.ino
  - Bench rover pipeline: receives correction bytes over UART and writes raw to GNSS.

## Reduced capability examples (UNO/Micro/Feather class)

- ReducedCommandConsole: examples/ReducedCommandConsole/ReducedCommandConsole.ino
  - No LC29H_GNSS object; sends core PQTM payloads with lightweight checksum builder.
  - Intended for memory-constrained boards where full feature examples are too heavy.
  - Commands: help, status, rover [rateMs], base, fixrate MS, hot/warm/cold, gnss_start/gnss_stop, send PAYLOAD.
- ReducedSerialBridge: examples/ReducedSerialBridge/ReducedSerialBridge.ino
  - Minimal raw byte bridge: USB Serial <-> GNSS UART.
  - Useful for pure pass-through diagnostics and external host tools.

Default serial wiring in reduced examples:

- Boards with Serial1 (Micro, many Feather-class boards): use Serial1 for GNSS.
- Boards without Serial1 (for example UNO): use SoftwareSerial on RX=4, TX=3.

## Bench pair: base + rover over simple serial wires

Use two Arduino boards and two GNSS modules:

- Board A runs BaseSerialBridge
- Board B runs RoverCorrectionBridge
- Both sketches are intended to use project config from lc29hconfig.h

Project config setup for bench pair:

1. For the base sketch, edit the example-local lc29hconfig.h and set LC29H_ROLE = LC29H_ROLE_BASE_SURVEY (or LC29H_ROLE_BASE_STATIC).

1. For the rover sketch, edit the example-local lc29hconfig.h and set LC29H_ROLE = LC29H_ROLE_UAS_ROVER.

1. Keep debug Serial enabled at 115200 on both boards.

Minimum wiring between boards for one-way correction transport:

- Board A link TX -> Board B link RX
- Board A GND -> Board B GND

Optional return path for rover-side serial data (if you enable rover link TX features):

- Board B link TX -> Board A link RX

Example default UART mapping in the examples:

- ESP32 base: GNSS on Serial1 and rover link on Serial2; default example GPIOs are RX18/TX17 and RX5/TX4, and the example-local lc29hconfig.h is the source of truth.
- ESP32 rover: GNSS on Serial1 and correction link on Serial2; avoid GPIO19/GPIO20 on ESP32-S3 boards when using native USB console unless you have verified the board wiring.
- Non-ESP32 fallback: SoftwareSerial GNSS on (RX4/TX3), link on (RX6/TX5)
- Non-ESP32 note: this leaves hardware Serial available for debug monitor.

Bring-up steps:

1. Flash BaseSerialBridge to base board and RoverCorrectionBridge to rover board.
2. Connect each board to its GNSS module UART.
3. Connect inter-board serial link and common ground.
4. Open both Serial Monitors at 115200.
5. Confirm base stats show RTCM frames and rover stats show correction bytes into GNSS.
6. Observe rover NMEA output and external app fix improvements as corrections arrive.

## Stream bridge example

- StreamBridge: examples/StreamBridge/StreamBridge.ino
  - Demonstrates forwarding GNSS output to an upstream stream for caster/bridge use.
  - Uses the library bridge parser API (forwardBridgeAvailable) rather than sketch-local parsing logic.
  - Bridge mode and NMEA allowlist are configurable from lc29hconfig.h.

## ESP32 base station example

- ESP32BaseStation: examples/ESP32BaseStation/ESP32BaseStation.ino
  - ESP32-only example with GNSS on Serial1 and RTCM/link output on Serial2.
  - Intended for ESP32-S3-DevKitC-1 compatible boards (including common clone variants).
  - UART GPIOs are defined in the example-local lc29hconfig.h, which is the source of truth for clone-specific pin routing.
  - The default example avoids GPIO19/GPIO20 because those are commonly used by ESP32-S3 native USB.
  - Uses the same project config flow as the generic examples, but removes non-ESP32 fallback code.
  - Intended as a hardware-specific starting point, not a full Wi-Fi/NTRIP application.

## ESP32 USB-UART bridge example

- ESP32UsbUartBridge: examples/ESP32UsbUartBridge/ESP32UsbUartBridge.ino
  - Uses native USB Serial for logs and interactive library commands.
  - Intended for ESP32-S3-DevKitC-1 compatible boards (including common clone variants).
  - Uses the CH340-backed second USB-C as Serial0 for raw GNSS traffic so host tools such as QGNSS can talk through the dev board.
  - Treat the example-local lc29hconfig.h as authoritative for clone-specific USB/UART routing.
  - For this board pinout, that CH340 UART path maps to U0RXD/U0TXD (GPIO44/GPIO43); keep those pins free in your own wiring.
  - Uses Serial1 header pins for the GNSS module itself.
  - Intended as a fast bring-up path when the ESP32-S3 board has dual USB-C and the GNSS module is wired only over UART.
  - Use one controller at a time: either QGNSS over CH340 or Arduino Serial console commands.
  - Optional strict ownership mode can disable local console commands while bridge mode is active.
  - Optional startup sanity checks verify module response path (version/mode/baud) before steady bridge operation.

Dual USB-C quick bring-up steps:

1. Wire GNSS UART to ESP32 UART1 pins: U1RXD (GPIO18), U1TXD (GPIO17), and GND.
2. Plug native USB-C into your console host and open Serial Monitor at 115200.
3. Plug CH340 USB-C into your QGNSS host and select the CH340 COM port.
4. Set QGNSS serial baud to match LC29H_CFG_ESP32_USB_UART_BRIDGE_BAUD in examples/ESP32UsbUartBridge/lc29hconfig.h.
5. Upload examples/ESP32UsbUartBridge/ESP32UsbUartBridge.ino and confirm the console prints "USB-UART bridge active.".
6. In QGNSS, query version/status through the CH340 COM path.
7. If you prefer console-only debug, leave QGNSS disconnected while the sketch is running and use native USB Serial Monitor output.

Useful bridge toggles in examples/ESP32UsbUartBridge/lc29hconfig.h:

1. LC29H_CFG_ESP32_USB_UART_BRIDGE_APPLY_PROJECT_CONFIG
2. LC29H_CFG_ESP32_USB_UART_BRIDGE_STRICT_OWNERSHIP
3. LC29H_CFG_ESP32_USB_UART_BRIDGE_STARTUP_SANITY_CHECKS

## ESP32 Bluetooth rover example

- ESP32BtRoamer: examples/ESP32BtRoamer/ESP32BtRoamer.ino
  - Accepts correction bytes from a phone app and feeds them to GNSS UART.
  - Board-dependent transport selection at compile time:
    - Classic ESP32 target (esp32:esp32:esp32): BT Classic SPP via BluetoothSerial.
    - ESP32-S3 target (esp32:esp32:esp32s3): BLE GATT ingress (NUS-style RX characteristic).
  - Bidirectional data path is enabled in this example:
    - Phone app -> rover: binary RTCM/RTIP corrections forwarded to GNSS UART.
    - Rover -> phone app: NMEA text lines sent over the active Bluetooth transport.
  - Keeps USB Serial console available for status, NMEA monitor output, and interactive commands.

Typical quick rover path:

1. Upload ESP32BtRoamer to an ESP32 rover board.
2. Pair your phone to the configured Bluetooth name (LC29H_CFG_ESP32_BT_NAME).
3. In your phone correction app, open the ESP32 Bluetooth link for your target transport (SPP on classic ESP32, BLE on ESP32-S3) and send RTCM/RTIP correction stream.
4. Confirm status lines show increasing btBytesRead/btBytesToGnss.
5. Confirm btNmeaLines is increasing to verify NMEA egress to the Bluetooth app.
6. Monitor local NMEA/fix behavior on USB Serial.

BLE transport details for ESP32-S3:

1. Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
2. RX characteristic UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
3. TX characteristic UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
4. Phone app -> rover: write raw binary RTCM/RTIP correction bytes to the RX characteristic.
5. Rover -> phone app: subscribe to TX notifications to receive NMEA text output.
6. BLE TX notifications are sent in small chunks with CRLF line endings so app-side software must reassemble full NMEA lines.
7. SW Maps compatibility depends on the Android-side app path supporting this NUS-style BLE read/write model, not just generic BLE discovery.

Important scope note:

- Caster setup (laptop/Raspberry Pi, NTRIP/caster software, network transport, credentials) is outside the scope of this library.
- This repository provides serial/Bluetooth transport examples only.

## Compile matrix (Arduino CLI)

Verified with:

- arduino-cli 1.5.1
- arduino:avr 1.8.8
- esp32:esp32 3.3.11

Repository release:

- v0.1.0

Results:

Generic/reduced examples:

| Target FQBN | BasicConfiguration | SimpleRover | SimpleBaseStation | StreamBridge | BaseSerialBridge | RoverCorrectionBridge | ESP32BaseStation | ESP32UsbUartBridge |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| arduino:avr:uno | FAIL (RAM overflow) | FAIL (RAM overflow) | FAIL (RAM overflow) | FAIL (RAM overflow) | FAIL (RAM overflow) | FAIL (RAM overflow) | N/A | N/A |
| arduino:avr:mega | PASS | PASS | PASS | PASS | PASS | PASS | N/A | N/A |

Reduced examples on constrained AVR targets:

| Target FQBN | ReducedCommandConsole | ReducedSerialBridge |
| --- | --- | --- |
| arduino:avr:uno | PASS | PASS |
| arduino:avr:micro | PASS | PASS |

ESP32 consolidated sweep (all ESP32-specific examples):

| Target FQBN | ESP32BaseStation | ESP32UsbUartBridge | ESP32BtRoamer |
| --- | --- | --- | --- |
| esp32:esp32:esp32 | PASS | PASS | PASS |
| esp32:esp32:esp32s3 | PASS | PASS | PASS (BLE transport path) |

Notes:

1. AVR Uno/Nano-class boards do not have enough RAM for these sketches as currently written.
2. Generic examples are verified on Mega-class AVR hardware.
3. ESP32-specific examples are validated on both esp32 and esp32s3 targets with Arduino CLI.
4. ESP32BtRoamer now selects Bluetooth transport by target capability: SPP when available, otherwise BLE.
5. GitHub Actions runs Arduino CLI compile validation on every push for the passing target set in these tables.
6. Full-size Uno/Nano example failures remain documented here but are intentionally excluded from CI because they are known SRAM-limit cases, not regressions.

## Project config header

This library supports a project-local configuration file named lc29hconfig.h.

Workflow:

1. For a custom sketch outside the examples tree, copy lc29hconfig.h.template from the library into your sketch/project root.
2. Rename it to lc29hconfig.h.
3. Set LC29H_ROLE and related values.
4. Include LC29H_ProjectConfig.h in your sketch.
5. Call LC29H_applyProjectConfig(gnss, result) at startup.

Optional bridge-related macros in lc29hconfig.h:

1. LC29H_CFG_BRIDGE_MODE
1. LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED
1. LC29H_CFG_BRIDGE_FORWARD_NMEA_GGA
1. LC29H_CFG_BRIDGE_FORWARD_NMEA_GST
1. LC29H_CFG_BRIDGE_FORWARD_NMEA_RMC
1. LC29H_CFG_BRIDGE_FORWARD_PQTM_STATUS
1. LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE

Optional rover-correction macros in lc29hconfig.h:

1. LC29H_CFG_ROVER_PRINT_LOCAL_NMEA
1. LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK
1. LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE

Optional recovery-policy macros in lc29hconfig.h:

1. LC29H_CFG_RECOVERY_COMMAND_RETRIES
1. LC29H_CFG_RECOVERY_QUERY_RETRIES
1. LC29H_CFG_RECOVERY_RAW_WRITE_RETRIES
1. LC29H_CFG_RECOVERY_RETRY_DELAY_MS
1. LC29H_CFG_RECOVERY_EMIT_EVENTS

Recommended bridge debug presets:

| Use case | LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE | LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED |
| --- | --- | --- |
| Quiet bench run | LC29H_CFG_LOCAL_DEBUG_OUTPUT_NONE | 1 |
| NMEA diagnostics | LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY | 1 |
| Raw binary visibility | LC29H_CFG_LOCAL_DEBUG_OUTPUT_RAW_BINARY | 1 |
| Full NMEA passthrough | LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY | 0 |

Benefits:

- One project-level file controls intended module role and key settings.
- Keeps examples and library portable while allowing per-project specialization.
- Easy to version-control as part of each user project.

## Serial commands

Use these commands in the sketch Serial Monitor (newline-terminated input).

Legend:

- REQUIRED_ARG = required argument
- [arg] = optional argument
- 0or1 = boolean flag (0=false, 1=true)

General and setup:

- help [command]
  - Syntax: help or help COMMAND_NAME
  - Purpose: print command list or detailed help for one command.
  - Example: help base_survey
- status
  - Syntax: status
  - Purpose: run a quick health/query bundle (version, mode, survey status, baud query).
- restore
  - Syntax: restore
  - Purpose: restore receiver defaults.
- save
  - Syntax: save
  - Purpose: save current configuration to receiver flash.

Role and profile control:

- rover [rateMs]
  - Syntax: rover [rateMs]
  - Purpose: set rover mode and optional fix/output rate (ms).
  - Example: rover 200
- base
  - Syntax: base
  - Purpose: set base mode.
- base_survey [minTimeSec] [stdDevM]
  - Syntax: base_survey [minTimeSec] [stdDevM]
  - Purpose: configure Survey-In base mode.
  - Example: base_survey 300 2.0
- base_fixed LAT LON ALT
  - Syntax: base_fixed LAT_DEG LON_DEG ALT_M
  - Purpose: configure fixed-base position from decimal lat/lon + altitude.
  - Example: base_fixed 47.6205 -122.3493 52.4
- profile_uas [fixMs] [save0or1] [verify0or1]
  - Syntax: profile_uas [fixMs] [save0or1] [verify0or1]
  - Purpose: apply UAS rover profile.
  - Example: profile_uas 200 1 1
- profile_base_survey [sec] [std] [rtcm0or1] [save0or1] [verify0or1]
  - Syntax: profile_base_survey [sec] [std] [rtcm0or1] [save0or1] [verify0or1]
  - Purpose: apply survey-base mission profile.
  - Example: profile_base_survey 3600 1.5 1 1 1
- profile_base_static LAT LON ALT [rtcm0or1] [save0or1] [verify0or1]
  - Syntax: profile_base_static LAT_DEG LON_DEG ALT_M [rtcm0or1] [save0or1] [verify0or1]
  - Purpose: apply static-base mission profile.
  - Example: profile_base_static 33.259933 -97.897003 276.0 1 1 1

Survey capture and conversion:

- survey_capture [timeoutMs]
  - Syntax: survey_capture [timeoutMs]
  - Purpose: query Survey-In response and capture ECEF coordinates.
  - Example: survey_capture 2000
- survey_pos
  - Syntax: survey_pos
  - Purpose: print captured Survey-In ECEF values.
- survey_apply [save0or1]
  - Syntax: survey_apply [save0or1]
  - Purpose: apply captured ECEF as fixed-base config.
  - Example: survey_apply 1
- survey_finalize [timeoutMs] [save0or1]
  - Syntax: survey_finalize [timeoutMs] [save0or1]
  - Purpose: capture Survey-In ECEF and apply it as fixed-base in one flow.
  - Example: survey_finalize 2000 1

Accuracy trend status (ring-buffer backed):

- survey_status [tailCount]
  - Syntax: survey_status [tailCount]
  - Purpose: print survey accuracy-tracker summary.
  - Optional tailCount prints recent samples as elapsedSec,estimate.
  - Example: survey_status 10
- rover_status [tailCount]
  - Syntax: rover_status [tailCount]
  - Purpose: print rover accuracy-tracker summary.
  - Optional tailCount prints recent samples as elapsedSec,estimate.
  - Example: rover_status 10

Queries and stream control:

- mode_query
  - Syntax: mode_query
  - Purpose: query receiver mode.
- msg_on NAME [port]
  - Syntax: msg_on MESSAGE_NAME [port]
  - Purpose: enable a message on a port.
  - Example: msg_on GGA 1
- msg_off NAME [port]
  - Syntax: msg_off MESSAGE_NAME [port]
  - Purpose: disable a message on a port.
  - Example: msg_off GGA 1
- msg_query NAME [port]
  - Syntax: msg_query MESSAGE_NAME [port]
  - Purpose: query message rate on a port.
  - Example: msg_query GGA 1
- baud RATE [port]
  - Syntax: baud BAUD_RATE [port]
  - Purpose: set receiver UART baud.
  - Example: baud 115200 1
- baud_query [port]
  - Syntax: baud_query [port]
  - Purpose: query receiver UART baud.
  - Example: baud_query 1
- fixrate MS
  - Syntax: fixrate MILLISECONDS
  - Purpose: set navigation fix interval.
  - Example: fixrate 200
- fixrate_query
  - Syntax: fixrate_query
  - Purpose: query current navigation fix interval.
- rtcm on|off
  - Syntax: rtcm on or rtcm off
  - Purpose: enable or disable RTCM outputs used in base workflows.

GNSS control and information:

- hot
  - Syntax: hot
  - Purpose: hot restart.
- warm
  - Syntax: warm
  - Purpose: warm restart.
- cold
  - Syntax: cold
  - Purpose: cold restart.
- gnss_start
  - Syntax: gnss_start
  - Purpose: start GNSS engine.
- gnss_stop
  - Syntax: gnss_stop
  - Purpose: stop GNSS engine.
- uid
  - Syntax: uid
  - Purpose: query receiver unique ID.
- qver
  - Syntax: qver
  - Purpose: query firmware/protocol version details.

Raw send:

- send PAYLOAD
  - Syntax: send PQTM_OR_PAIR_PAYLOAD_OR_SENTENCE_BODY
  - Purpose: send raw command payload through library sentence builder.
  - Example: send PQTMGNSSSTART

## Checksum and command builder behavior

Yes. The library already calculates checksums automatically when sending payloads.

- sendPayload("PQTMCFGRCVRMODE,W,1")
  - Automatically sends: $PQTMCFGRCVRMODE,W,1*XX\r\n

For command + variables workflows, you can now use:

- sendCommand(command, argsCsv)
- sendCommand(command, argsArray, argCount)

Example:

```cpp
String args[] = {"W", "1", "3600", "15", "0", "0", "0"};
gnss.sendCommand("PQTMCFGSVIN", args, 7);
// Sends: $PQTMCFGSVIN,W,1,3600,15,0,0,0*XX\r\n
```

You can also build without sending:

```cpp
String payload = LC29H_GNSS::buildPayload("PQTMCFGRCVRMODE", "W,2");
String sentence = LC29H_GNSS::makeSentence(payload);
```

## Raw passthrough helpers

For application stacks that need unparsed bytes:

- writeRaw(data, len)
  - Send raw bytes to module (for example RTCM corrections into rover).
- ingestRawAvailable(in, maxBytes, stats, chunkSize)
  - Pull available bytes from an input stream and write them to GNSS raw path.
  - Useful for rover correction links with byte counters and short-write visibility.
- forwardAvailable(out, maxBytes)
  - Forward all currently available bytes from GNSS stream to another stream.
- forwardNmeaLine(out, timeoutMs)
  - Forward one complete NMEA line (if next line is NMEA).
- forwardBridgeAvailable(out, state, mode, filter, stats, maxBytes, localNmeaOut)
  - Full RTCM/NMEA bridge parser with reusable state and mode/filter controls.
  - Supports NMEA filter enable/disable and optional raw local debug mirror stream.
  - When the NMEA allowlist is enabled, the local NMEA debug mirror only prints allowed lines.
- isNmeaAllowedByFilter(line, filter)
  - Reusable allowlist check for NMEA forwarding decisions.
- tryParsePairAck(line, ack)
  - Parse PAIR001 ACK lines into command ID and result code.
- pairAckResultName(result)
  - Decode PAIR ACK result codes to readable names.
- printPairAck(out, ack, label)
  - Standardized one-line PAIR ACK print helper for examples/apps.
- printBridgeStatus(out, label, mode, stats, uptimeMs)
  - Standardized bench status line for bridge telemetry.
- ingestRawAvailable(in, maxBytes, stats, chunkSize)
  - Standardized correction-ingress pump for rover raw input to GNSS.
- printRawIngressStatus(out, label, stats, uptimeMs)
  - Standardized bench status line for rover correction ingress telemetry.

Bridge mode enum values:

- BridgeMode::ForwardAll
- BridgeMode::RtcmOnly
- BridgeMode::RtcmAndNmeaAllowlist

Local bridge debug visibility modes (config-driven in bridge examples):

- None
- NmeaOnly
- RawBinary

These APIs make it easy to keep this library as control/config plane while a higher layer handles data plane parsing/transport.

RoverCorrectionBridge now uses ingestRawAvailable(...) to keep correction-ingress behavior centralized in the library.
BaseSerialBridge, StreamBridge, and RoverCorrectionBridge now print periodic status lines through library helpers for consistent bench diagnostics.

## Error handling and host callbacks

The library now exposes structured error state so host applications can react without parsing serial text:

- getLastError()
- getLastErrorContext()
- clearLastError()
- errorCodeName(...)
- printLastError(...)

Optional push-style event callback:

- setEventHandler(handler, userData)

Configurable recovery policy:

- setRecoveryPolicy(policy)
- getRecoveryPolicy()

Recovery policy fields:

- commandRetries
- queryRetries
- rawWriteRetries
- retryDelayMs
- emitRecoveryEvents

Event payload contains:

- type (Info, Warning, Error)
- code (ErrorCode)
- context (short operation detail)

Typical host flow:

1. Call an operation (for example profile apply or query helper).
1. If false/non-success, read getLastError() and getLastErrorContext().
1. Surface that to your app UI/log and decide retry/recovery policy.

Built-in recovery behavior:

- sendPayload retries stream writes using commandRetries.
- queryAndWaitLine retries send/wait cycles using queryRetries.
- ingestRawAvailable retries short raw writes using rawWriteRetries.

ACK parsing helper behavior:

- PAIR001 ACK lines can now be parsed and printed in a consistent format by examples or host code.

Example integration note:

- BasicConfiguration, SimpleRover, SimpleBaseStation, BaseSerialBridge, RoverCorrectionBridge, and StreamBridge now register an event handler and set a recovery policy at startup.

## Preset-level workflows

For higher-level setup, use these one-call methods:

- applyRoverPreset(outputMs, save)
  - Sets rover mode and fix rate before optional save.
- applyBaseSurveyPreset(minTimeSec, minStdDevM, enableRtcm, save)
- applyBaseFixedPreset(latDeg, lonDeg, altM, enableRtcm, save)

Each returns PresetResult:

- Success
- CommandFailed
- SaveFailed

And exposes a power-cycle advisory flag:

- isPowerCycleRecommended()
- clearPowerCycleRecommended()

Example:

```cpp
LC29H_GNSS::PresetResult r = gnss.applyBaseSurveyPreset(3600, 1.5f, true, true);
if (r == LC29H_GNSS::PresetResult::Success && gnss.isPowerCycleRecommended()) {
  Serial.println("Power-cycle recommended for full effect");
}
```

## Mission profile workflows

Mission profiles are higher-level wrappers that include optional verification query steps after configuration.

- applyUasRoverProfile(fixRateMs, save, verify)
- applySurveyBaseProfile(minTimeSec, minStdDevM, enableRtcm, save, verify)
- applyStaticBaseProfile(latDeg, lonDeg, altM, enableRtcm, save, verify)

Each returns ProfileResult with:

- status: Success | CommandFailed | SaveFailed | VerifyFailed
- powerCycleRecommended

Verification behavior:

- Profiles now perform field-level checks where available (mode, fix rate, key message rates, baud), not just send/query command-path checks.

Known limitation:

- The verification step checks that the configured queries return acceptable values, but it does not replace receiver-side ACK parsing for every command family.

Example:

```cpp
LC29H_GNSS::ProfileResult r = gnss.applyUasRoverProfile(200, true, true);
if (r.status == LC29H_GNSS::ProfileStatus::Success && r.powerCycleRecommended) {
  Serial.println("UAS profile applied. Power-cycle recommended.");
}
```

## Survey-in to fixed ECEF workflow

This library now supports a portable, hardware-agnostic path for preserving exact surveyed position:

1. Query survey-in config/status and capture ECEF X/Y/Z from response
2. Store captured ECEF in library variables for user management
3. Write captured ECEF back to module as fixed base position
4. Save configuration and power-cycle when recommended

Primary APIs:

- querySurveyInAndCaptureEcef(timeoutMs)
- hasCapturedSurveyEcef()
- getCapturedSurveyEcef()
- applyCapturedSurveyEcefAsFixed(save)
- finalizeSurveyInToFixedBase(timeoutMs, save)

Portable storage note:

The library intentionally only holds captured ECEF in RAM. Persisting to flash/EEPROM/Preferences is platform-specific (for example ESP32 NVS) and left to application code so examples stay portable across Arduino targets.

## Data source and traceability

Command seeds came from:

- Temp/Quectel GPS Documents/Software/QGNSS_V2.5_EN/Config/ModeInfo.json
- Temp/Quectel GPS Documents/Software/QGNSS_V2.5_EN/logFile/historyLogFile/.../QGNSS(1).log

Those command seeds are now captured in CommandReference.md so Temp can be removed later without losing the initial command catalog.

Protocol PDFs in Temp should remain the final authority before locking API behavior.

## Next build targets

- Parse and surface PAIR001 ACK responses and error codes
- Add explicit save command verification per module/firmware
- Add tested RTCM preset helpers (1005/1077/1087/etc.)
- Add platform examples for ESP32, AVR, and dual-UART boards
- Add unit tests for checksum and coordinate conversion
