# LC29H Command Reference (Observed)

This file captures commands observed in QGNSS logs/config and wired into the initial library.

Known limit:

- PQTMCFGBASE is log-backed here and remains provisional until module-specific ACKs confirm it on your target firmware.

## Family traceability

The V1.5 protocol evaluation identified 108 unique command bases. This reference groups the observed commands by functional block so the remaining command bases can be added into the right family without changing the transport layer.

| Family | Purpose | Current coverage |
| --- | --- | --- |
| Identity | Version, firmware, unique ID, serial number | Dedicated wrappers for the currently used identity queries |
| Lifecycle | Restore/save, GNSS start/stop, hot/warm/cold restart | Dedicated wrappers and console commands exist |
| Core configuration | Receiver mode, fix rate, baud, protocol mask, constellations, NMEA formatting | Dedicated wrappers exist for the current setup workflow |
| Survey and base | Survey-in, fixed base, ECEF capture/apply | Dedicated wrappers exist for survey and fixed-base workflows |
| Output and diagnostics | PVT, velocity, DOP, STD, jamming, geofence, odometer | Query wrappers exist for the currently exposed diagnostic set |
| Pair control | PAIR* commands and acknowledgments | ACK parsing helpers exist; the rest still use generic send helpers |
| Pair control extras | PAIR001 response capture | `readPairAck()` can parse and wait for ACK lines |
| Transport and bridge | Raw byte ingress/egress and bridge forwarding | Dedicated bridge helpers exist |
| Console and utility | Serial Monitor command processor and helper utilities | Implemented in the library and example sketches |
| Generic | Remaining protocol commands not yet wrapped one-by-one | Still reachable through `sendPayload()`, `sendSentence()`, and `sendCommand()` |

## Metadata schema

Each command family now has a common metadata shape so the rest of the protocol surface can be added with consistent fields.

Tracked fields:

- base command
- wrapper name
- family
- direction
- ACK kind
- summary
- firmware gate
- module gate
- save recommended
- power-cycle recommended
- generic fallback
- request field specs
- response field specs
- response prefix and notes

The family template defines the default shape and individual commands only override the fields that differ.

Use `help family <name>` in the Serial Monitor to print the default schema for a family such as `identity`, `lifecycle`, `coreconfiguration`, `survey`, `output`, `bridge`, or `pair`.

## Observed from Temp/QGNSS files

### Identity

- PQTMVERNO
  - Purpose: Query module firmware/variant.
  - Example: $PQTMVERNO*58

- PQTMQVER
  - Purpose: Query firmware/protocol version details.

- PQTMUNIQID
  - Purpose: Query module unique ID.

- PQTMSN
  - Purpose: Query serial number or similar identity data depending on firmware.

- querySerialNumber()
  - Purpose: Dedicated wrapper for `PQTMSN`.

### Lifecycle

- PQTMRESTOREPAR
  - Purpose: Restore configuration defaults.
  - Example in logs appears without checksum while typing.

- PQTMSAVEPAR
  - Purpose: Save the current configuration to flash.

- PQTMHOT
  - Purpose: Perform a hot restart.

- PQTMWARM
  - Purpose: Perform a warm restart.

- PQTMCOLD
  - Purpose: Perform a cold restart.

- PQTMGNSSSTART
  - Purpose: Start the GNSS engine.

- PQTMGNSSSTOP
  - Purpose: Stop the GNSS engine.

### Core configuration

- PQTMCFGRCVRMODE,W,1
  - Purpose: Set receiver mode to rover.

- PQTMCFGRCVRMODE,W,2
  - Purpose: Set receiver mode to base.

- PQTMCFGSVIN,W,1,TIME_SEC,ACC_LIMIT_M,0,0,0
  - Purpose: Configure Survey-In. `<3D_AccLimit>` is **meters** (not tenths). Quectel default 15.0 starts `<Obs>` on LC29H(DA); 0 is not usable.
  - Observed examples:
    - $PQTMCFGSVIN,W,1,3600,15,0,0,0*1
    - $PQTMCFGSVIN,W,1,84600,15,0,0,0*1C
  - DA/EA: takes effect only after PQTMSAVEPAR and PAIR023 (`rebootModule()`). PAIR003/PAIR002 GNSS sleep is not enough.

- PQTMCFGSVIN,R
  - Purpose: Read Survey-In config/status.

- PQTMCFGMSGRATE,W,MESSAGE_NAME,RATE
  - Purpose: Configure message output rate on the receiver's configured output port.
  - Observed example:
    - $PQTMCFGMSGRATE,W,PQTMSVINSTATUS,1*58

- PQTMCFGFIXRATE,W,MS
  - Purpose: Configure rover/fix output interval.

- PQTMCFGUART
  - Purpose: Configure UART port parameters.

- PQTMCFGPROT
  - Purpose: Configure protocol input/output masks.

- PQTMCFGCNST
  - Purpose: Configure constellation enable/disable state.

- PQTMCFGNAVMODE
  - Purpose: Configure navigation mode.

- PQTMCFGNMEADP
  - Purpose: Configure NMEA decimal precision.

- PQTMCFGNMEATID
  - Purpose: Configure NMEA talker ID mode.

- PQTMCFGPPS
  - Purpose: Configure pulse-per-second output behavior.

- setPulsePerSecondConfig(argsCsv)
  - Purpose: Dedicated wrapper for `PQTMCFGPPS,W,...`.

- queryPulsePerSecondConfig()
  - Purpose: Dedicated wrapper for `PQTMCFGPPS,R`.

### Survey and base

- PQTMCFGBASE,1,LAT_DDMM_MMMM,NS,LON_DDDMM_MMMM,EW,ALT_M
  - Purpose: Configure base fixed position.
  - Observed example:
    - $PQTMCFGBASE,1,3315.5960,N,09753.8202,W,276*2E
  - Note: this command is currently documented from QGNSS logs and remains provisional until confirmed against module-specific ACKs.

- PAIR432,1 and PAIR434,1
  - Purpose: Enable RTCM MSM7 + 1005 (1 Hz mission stream for a base).

- PAIR432,-1 and PAIR434,0
  - Purpose: Disable RTCM. `PAIR432,0` is MSM4, not off.

- PQTMCFGSVIN,W,1,TIME_SEC,ACC_LIMIT_M,0,0,0
  - Purpose: Configure Survey-In. AccLimit is meters; 15 starts `<Obs>` on DA.

- PQTMCFGSVIN,R
  - Purpose: Read Survey-In config/status.

- PQTMSVINSTATUS
  - Purpose: Report Survey-In status.

- PQTMCFGMSGRATE,W,MESSAGE_NAME,PORT,RATE
  - Purpose: Configure message output rate.

- PQTMCFGMSGRATE,R,MESSAGE_NAME,PORT
  - Purpose: Query message output rate.

### Output and diagnostics

- PQTMPVT
  - Purpose: Query position/time solution data.

- PQTMVEL
  - Purpose: Query velocity data.

- PQTMSTD
  - Purpose: Query standard deviation data.

- PQTMDOP
  - Purpose: Query dilution of precision data.

- PQTMJAMMINGSTATUS
  - Purpose: Query jamming/interference status.

- PQTMGEOFENCESTATUS
  - Purpose: Query geofence status.

- PQTMMEPE
  - Purpose: Query estimated positioning error metrics.

### Pair control

- PAIR001
  - Purpose: ACK/error response parsing target for pair-control flows.

- readPairAck(outAck, timeoutMs)
  - Purpose: Wait for and parse a PAIR001 ACK line from the GNSS stream.

## Also present in QGNSS ModeInfo.json

- PAIR004, PAIR005, PAIR006, PAIR007
  - These appear as hot/warm/cold/full/restart commands in tool config and can be exposed in later API passes.

- PAIR023
  - Purpose: Full module reboot. Wrapper: `rebootModule()`.
  - Required on LC29H(DA/EA) after PQTMSAVEPAR for CFGSVIN (survey-in) to start counting `<Obs>`.
  - PAIR003/PAIR002 GNSS sleep is not a reboot.

## Important note

For commands not yet fully validated against protocol PDFs, keep usage in bench-test mode and verify receiver responses (for example with PAIR001 ACK and status lines).

## Finalization status

This repository now has three classes of command coverage:

- Verified: commands present in the checked-in references and wired into the library.
- Provisional: commands observed in logs or tool output but not yet confirmed against the V1.5 protocol text on the target firmware.
- TODO placeholders: the remaining V1.5 slots that are known to exist in the protocol evaluation but are not recoverable from the checked-in repository alone.

Current placeholder policy:

- Keep the unresolved V1.5 slots in the registry as numbered placeholders.
- Do not assign new command identities to those slots unless they come from the canonical Temp extraction or live module validation.
- Treat PQTMCFGBASE and PQTMMEPE as provisional until they are confirmed on the target firmware.

## Temp deprecation note

This reference intentionally copies the command seed set into the repository root so the Temp directory can be removed after initial development without losing command provenance.
