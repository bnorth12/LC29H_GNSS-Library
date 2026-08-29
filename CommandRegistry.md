# LC29H Normalized Command Registry

Source notes:

- Normalized from repository references in `Readme.md`, `CommandReference.md`, `ProtocolV15_Evaluation.md`, `LC29H_GNSS.h`, `LC29H_GNSS.cpp`, and example sketches.
- The checked-in references currently expose 40 normalized command bases plus 1 transport helper row.
- The protocol evaluation documents 108 unique bases in V1.5, so 68 slots remain unresolved until the canonical command-bases file is checked in.

Status key:

- `known` = present in the checked-in references and mapped to metadata
- `provisional` = present in references but not fully protocol-confirmed
- `missing` = reserved slot for a V1.5 base that is not recoverable from the checked-in references yet

## Identity

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PQTMVERNO | known | `queryVersion()` | Identity | Read | DirectData | Module firmware/variant query |
| PQTMQVER | known | `queryQVersion()` | Identity | Read | DirectData | Firmware/protocol version query |
| PQTMUNIQID | known | `queryUniqueId()` | Identity | Read | DirectData | Unique ID query |
| PQTMSN | known | `querySerialNumber()` | Identity | Read | DirectData | Serial-number query |

## Lifecycle

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PQTMRESTOREPAR | known | `restoreDefaults()` | Lifecycle | Control | CommandOkError | Restore defaults |
| PQTMSAVEPAR | known | `saveConfig()` | Lifecycle | Control | CommandOkError | Save config |
| PQTMHOT | known | `hotStart()` | Lifecycle | Control | CommandOkError | Hot restart |
| PQTMWARM | known | `warmStart()` | Lifecycle | Control | CommandOkError | Warm restart |
| PQTMCOLD | known | `coldStart()` | Lifecycle | Control | CommandOkError | Cold restart |
| PQTMGNSSSTART | known | `startGnss()` | Lifecycle | Control | CommandOkError | Start GNSS |
| PQTMGNSSSTOP | known | `stopGnss()` | Lifecycle | Control | CommandOkError | Stop GNSS |

## Core Configuration

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PQTMCFGRCVRMODE | known | `setReceiverModeRover()/setReceiverModeBase()/queryReceiverMode()` | CoreConfiguration | ReadWrite | StatusLine | Rover/base mode |
| PQTMCFGMSGRATE | known | `setMessageRate()/queryMessageRate()/enableMessageOutput()/disableMessageOutput()` | CoreConfiguration | ReadWrite | StatusLine | Message rates |
| PQTMCFGFIXRATE | known | `setFixRateMs()/queryFixRate()` | CoreConfiguration | ReadWrite | StatusLine | Fix interval |
| PQTMCFGUART | known | `setBaudRate()/queryBaudRate()` | CoreConfiguration | ReadWrite | StatusLine | UART config |
| PQTMCFGPROT | known | `setProtocolMask()/queryProtocolMask()` | CoreConfiguration | ReadWrite | StatusLine | Protocol mask |
| PQTMCFGCNST | known | `setConstellations()/queryConstellations()` | CoreConfiguration | ReadWrite | StatusLine | Constellation selection |
| PQTMCFGNAVMODE | known | `setNavMode()/queryNavMode()` | CoreConfiguration | ReadWrite | StatusLine | Nav mode |
| PQTMCFGNMEADP | known | `setNmeaPrecision()/queryNmeaPrecision()` | CoreConfiguration | ReadWrite | StatusLine | NMEA precision |
| PQTMCFGNMEATID | known | `setNmeaTalkerId()/queryNmeaTalkerId()` | CoreConfiguration | ReadWrite | StatusLine | NMEA talker ID |
| PQTMCFGPPS | known | `setPulsePerSecondConfig()/queryPulsePerSecondConfig()` | CoreConfiguration | ReadWrite | StatusLine | PPS config |

## Survey and Base

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PQTMCFGSVIN | known | `configureBaseSurveyIn()/querySurveyIn()/getSurveyInConfig()/querySurveyInAndCaptureEcef()/finalizeSurveyInToFixedBase()` | SurveyAndBase | ReadWrite | StatusLine | Survey-In workflow; getSurveyInConfig for adopt |
| PQTMCFGBASE | provisional | `configureBaseFixed()` | SurveyAndBase | Write | StatusLine | Log-backed fixed-base path |

## Output and Diagnostics

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PQTMPVT | known | `queryPvt()` | OutputAndDiagnostics | Read | DirectData | PVT query |
| PQTMVEL | known | `queryVelocity()` | OutputAndDiagnostics | Read | DirectData | Velocity query |
| PQTMSTD | known | `queryStd()` | OutputAndDiagnostics | Read | DirectData | Std-dev query |
| PQTMDOP | known | `queryDop()` | OutputAndDiagnostics | Read | DirectData | DOP query |
| PQTMJAMMINGSTATUS | known | `queryJammingStatus()` | OutputAndDiagnostics | Read | DirectData | Jamming status |
| PQTMGEOFENCESTATUS | known | `queryGeoFenceStatus()` | OutputAndDiagnostics | Read | DirectData | Geofence status |
| PQTMODO | known | `queryOdometer()` | OutputAndDiagnostics | Read | DirectData | Odometer query |
| PQTMSVINSTATUS | known | via message-rate/config helpers | OutputAndDiagnostics | Read | DirectData | Survey-In status |
| PQTMMEPE | provisional | no dedicated wrapper | OutputAndDiagnostics | Read | DirectData | Estimated error query |

## Pair Control

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| PAIR001 | known | `readPairAck()/tryParsePairAck()` | PairControl | Read | PairAck | ACK parsing target |
| PAIR004 | known | generic send helper | PairControl | Control | PairAck | Hot/warm/cold/full/restart family member |
| PAIR005 | known | generic send helper | PairControl | Control | PairAck | Tool-config family member |
| PAIR006 | known | generic send helper | PairControl | Control | PairAck | Tool-config family member |
| PAIR007 | known | generic send helper | PairControl | Control | PairAck | Tool-config family member |
| PAIR023 | known | `rebootModule()` | PairControl | Control | PairAck | Full module reboot; required after SAVEPAR for DA survey-in |
| PAIR062 | known | `setPairNmeaOutputRate()` | PairControl | Write | PairAck | NMEA family rate; Type 3=GSV all talkers; DA Type 0–5 only |
| PAIR063 | known | `queryPairNmeaOutputRate()` | PairControl | Read | PairAck | Get NMEA family rate |
| PAIR432 | known | `enableRTCM(true/false)` | PairControl | Control | PairAck | RTCM enable control |
| PAIR433 | known | `queryRtcmOutputMode()` | PairControl | Read | PairAck | Get MSM4/MSM7/-1 |
| PAIR434 | known | `enableRTCM(true/false)` | PairControl | Control | PairAck | RTCM enable control |
| PAIR435 | known | `queryRtcmAntennaPoint()` | PairControl | Read | PairAck | Get RTCM 1005 enable |
| PAIR437 | known | `queryRtcmEphemerisOutput()` | PairControl | Read | PairAck | Get RTCM eph enable |
| PAIR864 | known | `setBaudRatePair()` | PairControl | Write | PairAck | DA/EA UART baud (v1.5: PQTMCFGUART is AA/AL); reboot to apply |
| PAIR865 | known | `queryBaudRatePair()` | PairControl | Read | PairAck | Get UART baud via PAIR |

## Transport and Bridge

| Base | Status | Wrapper | Family | Direction | ACK | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| raw-passthrough | known | `writeRaw()/ingestRawAvailable()/forwardAvailable()/forwardBridgeAvailable()` | TransportBridge | Control | None | Bridge helpers |

## Missing V1.5 slots

The following slots are reserved TODO placeholders for the remaining protocol bases that are referenced in the protocol evaluation but not recoverable from the checked-in docs/code alone.

Placeholder policy:

- These rows are reservation markers, not confirmed command identities.
- Only replace them with real command bases from the canonical Temp extraction or live validated module evidence.
- Keep provisional commands such as PQTMCFGBASE and PQTMMEPE outside this placeholder block until they are fully confirmed.

| Slot | Status | Family | Notes |
| --- | --- | --- | --- |
| V1.5-001 through V1.5-068 | missing | mixed | Populate after the canonical normalized command list is checked in |
