# LC29H_GNSS

[![Arduino CLI Validate](https://github.com/bnorth12/LC29H_GNSS-Library/actions/workflows/arduino-cli-validate.yml/badge.svg)](https://github.com/bnorth12/LC29H_GNSS-Library/actions/workflows/arduino-cli-validate.yml)

Configuration-focused Arduino library for Quectel **LC29H(BA), LC29H(BS), LC29H(DA), and LC29H(EA)**. Wrappers exist for the union of those ICDs; not every command is legal on every variant (see **Module variants** below).

## Module variants

Protocol sources: LC29H&LC79H Series GNSS Protocol Specification **v1.5** (BA/DA/EA and AA/CA/AL notes) and LC29H(BS) GNSS Protocol Specification **v1.1**.

| Variant | Role | Survey-in | RTCM | Baud | NMEA rates | Do not use on this variant |
| --- | --- | --- | --- | --- | --- | --- |
| **DA** | RTK **base** (this ESP32 app) | `PQTMCFGSVIN` + SAVEPAR + **PAIR023**. AccLimit **15**. Fix interval **1000 ms only**. | PAIR432 MSM7, PAIR434 1005; query 433/435 | **PAIR864** (v1.5 lists `PQTMCFGUART` as AA/AL). Reboot to apply. | `PQTMCFGMSGRATE` or PAIR062 Types **0–5** only. GSV is one family (all talkers). | PAIR066/080/100, `PQTMCFGNMEATID`, `PQTMCFGPROT`, `PQTMGETUTC` |
| **EA** | RTK **rover** (DR); same app family as DA | Same CFGSVIN path as DA if used as base. Fix interval **100–1000 ms**. PAIR062 OutputRate **0 or 1 only**. | RTCM **in** (MSM) + optional out | PAIR864 | CFGMSGRATE; keep GGA/RMC every epoch | AA/AL-only PAIR (066, 070–073, 104, 420, …) |
| **BA** | DR rover | CFGSVIN listed for BA in v1.5 | PAIR432 family | PAIR864 / PQTM where accepted | CFGMSGRATE | Do not assume DA 1 Hz-only; `PQTMGETUTC` / `PQTMQVER` are BA/CA |
| **BS** | Kit base (LoRa in-module). **Separate 27-page ICD.** | CFGSVIN, SAVEPAR, SVINSTATUS, EPE, CFGMSGRATE | PAIR432–437 **only** (no 023 in the BS book) | **PAIR864** (in the BS book) | CFGMSGRATE | Full v1.5 PAIR set (050, 062, 023, 752, …) is **not** in the BS spec — do not send them on BS |

Shared and worth exposing in the library for DA/EA/BS bases: CFGSVIN, SAVEPAR, CFGMSGRATE, PAIR432/433/434/435/436/437, PAIR864. PAIR023 is **not** in v1.5 chapter 2.4 and **not** in the BS book; it remains a DA/EA field command (`rebootModule()`).

`applyBaseStatusRates()` is the DA/EA base table (GSV RATE 20, GSA 8, GLL/VTG/GNS/GRS/ZDA **off**). Rover GIS rates stay in `applyRoverGisRates()` (EA/BA).

## Current status

Initial library skeleton is now implemented with:

- GNSS transport on any Arduino Stream
- Automatic NMEA-style checksum generation on TX (`makeSentence` / `nmeaChecksum`) and a public RX check (`hasValidNmeaChecksum`) so apps do not duplicate XOR/`*HH` logic
- Command send helpers for PQTM and PAIR payloads
- Receiver mode helpers
  - Rover: PQTMCFGRCVRMODE,W,1
  - Base: PQTMCFGRCVRMODE,W,2
- Rover output/fix rate helper: PQTMCFGFIXRATE,W/R
- Receiver mode query: PQTMCFGRCVRMODE,R
- Survey-In helper
- Message output control and query: PQTMCFGMSGRATE,W/R (writes use MsgName, Rate, MsgVer)
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

- `LC29H_UartPump`: drain ring (overwrite-oldest), RTCM FIFO, latest-wins NMEA mailboxes, GSV group aging. Priority 0/1/2 is a table the sketch fills (`baseStationPriorities()` / `roverGisPriorities()`).

This starter intentionally focuses on reliable command transport and reproducible setup flow first.

What each PQTM/PAIR/NMEA/RTCM item does in a base or rover sketch (RATE, AccLimit, PAIR023 vs PAIR003, which example uses which) is in **[Module messages in practice](#module-messages-in-practice)**.

Raw-data design note:

- This library is intentionally compatible with higher-layer NMEA/RTK stacks.
- It can be used for configuration/control while raw GNSS/RTCM bytes are forwarded upstream.
- Parsing can remain in dedicated libraries/apps (for example TinyGPS++, RTK parsers, SW Maps feeders, NTRIP bridge apps).

## Version 0.2.13

CI: SimpleBaseStation and SimpleRover now skip `processSerialCommands()` on Mega, same as the other full AVR examples (help strings were ~11 kB of `.data`). `LC29H_UartPump` stays ESP32-only so the 8 kB drain ring is not built for AVR.

## Version 0.2.12

UART drain-then-mailbox pump for mixed NMEA+RTCM (the old `forwardBridgeAvailable` path still exists):

- `LC29H_UartPump::Pump` copies UART bytes into an 8 kB overwrite-oldest ring (`drain`, 4 ms budget), then frames off that copy (`frame`). The UART driver is emptied without XOR/`String`/handlers on the read path.
- RTCM goes to a 6×1100 FIFO (drop-oldest, counted as `rtcmDrops`). Status NMEA goes to latest-wins mailboxes (RMC/GGA/SVIN/GST/GSA/EPE). GSV is one group slot (16 lines, all talkers in one epoch). A new epoch is a talker restart (`$GPGSV,...,1` after GPGSV already collected), not each constellation's first sentence. An unread epoch overwrite ages GSV after `gsvSkipLimit` (default 2, ~40 s at RATE 20). Aged GSV is delivered after RTCM and needed status, never as level 0.
- `hasValidNmeaChecksum(const char*)` does not allocate. Use it on the UART path. The `String` overload calls it.
- Sketch policy lives in `PriorityTable` (`baseStationPriorities`: RTCM=0, RMC/GGA/SVIN=1, GSV/GSA/GST/EPE=2). Call `drain` every loop tick; `processRtcm` then `processNmea` so crash breadcrumbs can split the phases.

Failure counters the host should log: `drainOverruns` (ring lost oldest bytes), `drainBudgetHits` (4 ms drain left UART bytes behind), `rtcmDrops`, `mailboxOverwrites`, `gsvDueCount` / `gsvSkipped`, `checksumFails`, `resyncs`.

## Version 0.2.11

Public `LC29H_GNSS::hasValidNmeaChecksum` so applications do not duplicate NMEA XOR/`*HH` on RX.

## Version 0.2.9

Library Readme **Module messages in practice**: what each PQTM/PAIR/NMEA/RTCM item does in a base or rover sketch, RATE vs Hz, DA survey-in gotchas, and which example uses which payload. Example READMEs list the subset that sketch sends.

## Version 0.2.8

Helpers the examples use for DA survey-in and UART budget:

- `getSurveyInConfig()` / `tryParseSvinStatus()` so a matching live survey-in can be adopted without PAIR023.
- `LC29H_MessageSchedule.h`: base status NMEA table, rover GIS table (GGA/RMC every epoch, GSV ~10 s).
- `LC29H_bringUp()` in `LC29H_ProjectConfig.h` applies that path for the configured role.

How each sketch uses those helpers is in `examples/README.md` and the README next to each `.ino`.

## Version 0.2.7

Survey-in and example bring-up aligned with LC29H(DA) field use:

- Default AccLimit for `configureBaseSurveyIn`, `applyBaseSurveyPreset`, and `applySurveyBaseProfile` is **15 m**. Values like 0 / 1.5 / 2 left `<Obs>` at 0 on DA.
- New `rebootModule()` sends `PAIR023` (full module reboot). DA/EA need `PQTMSAVEPAR` then `PAIR023` for CFGSVIN to take effect; `PAIR003`/`PAIR002` GNSS sleep is not enough.
- Console commands: `reboot`, and `base_survey` / `profile_base_survey` default AccLimit 15.
- Examples stay simpler than a full NTRIP app, but they now: AccLimit 15, do not auto-finalize survey-in on startup, lower GSV/`PQTMSVINSTATUS` to RATE 10, pump the UART every loop, and keep RTCM at 1 Hz.

## Version 0.2.6

LC29H(DA) message-rate and survey-in notes:

- `setMessageRate` omits `<MsgVer>` for standard NMEA (`GGA`, `GST`, …). `$PQTM` names still send MsgVer (`1`, or `2` for `PQTMEPE`).
- `configureBaseSurveyIn` still only writes `PQTMCFGRCVRMODE,W,2` and `PQTMCFGSVIN,W,1,…`. On DA/EA those take effect after `PQTMSAVEPAR` and **`rebootModule()` / `PAIR023`** (full module reboot). `PAIR003`/`PAIR002` GNSS sleep is not enough. Default AccLimit is **15 m**. The helper does not save or restart.

## Version 0.2.5

Command payloads aligned with Quectel LC29H(BS) Protocol Specification v1.1:

- `configureBaseSurveyIn` sends `<3D_AccLimit>` in **meters** (not tenths of a meter).
- `setMessageRate` writes `$PQTMCFGMSGRATE,W,<MsgName>,<Rate>,<MsgVer>` (MsgVer 1, or 2 for PQTMEPE).
- `enableRTCM(false)` uses `PAIR432,-1` to disable RTCM (0 means MSM4, not off).

## Version 0.2.4

Bridge pump hardening for mixed NMEA+RTCM on ESP32:

- `nmeaLine` is `reserve()`d to `kMaxBridgeNmeaChars` before per-byte appends, so a capped line
  does not realloc the Arduino `String` on every character.
- `_observeLineForAccuracy` is skipped when `localNmeaOut` is set. The host callback already
  parses the sentence; running the library tracker on the same stack was nesting two String-heavy
  walks inside `forwardBridgeAvailable`.
- `forwardBridgeAvailable` always returns after `kMaxBridgePumpMs` (15 ms) even when `maxBytes`
  is 0, so a slow callback cannot keep `while (available)` running until heap/stack collapse.
  Remaining UART bytes are picked up on the next call.

## Version 0.2.3

Bridge parser hardening (mixed NMEA+RTCM on one UART):

- `forwardBridgeAvailable` no longer grows `BridgeState::nmeaLine` without a bound. A false NMEA
  start (`$`/`!` appearing inside RTCM) previously appended every subsequent byte until a newline,
  which can exhaust heap and panic an ESP32 before the sketch loop runs again.
- NMEA assembly is now capped at `LC29H_GNSS::kMaxBridgeNmeaChars` (256). A non-ASCII byte, or a
  line that hits the cap, aborts the run, counts `BridgeStats::nmeaLineResyncs`, and resyncs: `0xD3`
  starts an RTCM frame, `$`/`!` starts a new NMEA line, anything else returns to Idle.
- `readLine` uses the same 256-character cap so a blocking query cannot swallow an RTCM burst into
  an unbounded `String`.

## UART throughput budget

The GNSS UART is a **single shared pipe**. Mixed NMEA + RTCM (especially MSM7 plus multi-constellation GSV) will overrun a typical ESP32 RX buffer long before it saturates the baud rate, if the sketch does not drain bytes faster than they arrive.

For a base station that publishes corrections, **RTCM at the nav epoch (typically 1 Hz MSM + 1005) is the mission stream**. NMEA is status. If the UART budget is exceeded, lower NMEA `PQTMCFGMSGRATE` (GSV first). Do not slow or drop RTCM to make the dashboard prettier.

Treat this as a real-time budget, not as “the parser will keep up.”

### Link capacity

UART 8N1 is 10 bit times per byte (start + 8 data + stop).

```
uart_bytes_per_s = baud / 10
```

| Baud   | Max payload bytes/s |
|--------|---------------------|
| 9600   | 960                 |
| 38400  | 3840                |
| 57600  | 5760                |
| 115200 | 11520               |
| 230400 | 23040               |

If `ingress_bytes_per_s` exceeds this table, bytes are lost on the wire. In practice ESP32 software RX buffers (256 default, 1024 in many sketches) overflow first.

### Ingress (what the module emits)

For each enabled sentence or RTCM message:

```
ingress_i = typical_size_i * rate_hz_i
ingress   = sum(ingress_i)
```

`PQTMCFGMSGRATE` `<Rate>` is **every N navigation epochs**. At a 1 Hz fix (`PQTMCFGFIXRATE` 1000 ms):

| Rate | Output interval |
|------|-----------------|
| 1    | 1 s             |
| 5    | 5 s             |
| 10   | 10 s            |

Typical sizes (order-of-magnitude, enough for budgeting):

| Output | Bytes per epoch (approx.) | Notes |
|--------|---------------------------|--------|
| GGA, RMC, GST, PQTMEPE | 70–90 each | One sentence each |
| GSA | ~70 × talkers | GPS+GLO+GAL+BDS often 4–5 sentences |
| GSV | **800–1200** | Many sentences, all constellations; the bulky NMEA item |
| PQTMSVINSTATUS | ~100 | Survey-in status |
| RTCM 1005 | ~25 | ARP |
| RTCM MSM7 (multi-GNSS) | **500–1500** | Dominates when base mode RTCM is on |

Worked example, **everything at 1 Hz** on a base with MSM7:

- NMEA: GGA+RMC+GSA+GSV+GST+SVIN+EPE ≈ 0.08+0.08+0.35+1.0+0.08+0.10+0.07 ≈ **1.8 kB/s**
- RTCM MSM7+1005 ≈ **0.8–1.5 kB/s**
- Total ≈ **2.6–3.3 kB/s**

That is only ~25–30% of 115200 baud, so the **baud rate is not the bottleneck**. The bottleneck is draining a 1 kB buffer between loop ticks.

### Buffer and pump constraints

```
bytes_arriving_between_pumps = ingress_bytes_per_s * pump_interval_s
```

This must stay below the UART RX software buffer (plus the small HW FIFO, 128 bytes on ESP32-S3) or the driver reports `UART_FIFO_OVF` / `UART_BUFFER_FULL` and drops data.

For a mixed RTCM+NMEA sketch on ESP32, prefer `LC29H_UartPump` over `forwardBridgeAvailable`:

1. `drain()` every loop — `available()` + `read()` only, 4 ms cap, 8 kB overwrite-oldest ring. `HardwareSerial::setTimeout(0)` so a blocked read cannot stall 1 s.
2. `frame()` XOR-checks NMEA and CRC-sizes RTCM on the ring copy, not on the UART driver.
3. `processRtcm` then `processNmea` with a short budget. RTCM is FIFO; status is latest-wins. GSV ages after two missed groups.

`forwardBridgeAvailable` remains for simple bridges. It:

- Stops after `maxBytes` **or** `kMaxBridgePumpMs` (15 ms), whichever comes first.
- Leftover bytes stay in the Stream; the **caller must invoke the pump again**.
- Bytes drained per call ≈ `min(maxBytes, time_in_callback_limited_read)`.

**Processing is part of the budget.** If the `localNmeaOut` callback parses `String`s, walks survey history, or writes LittleFS, drain rate collapses (hundreds of bytes in 15 ms instead of a full `maxBytes`). Then:

```
bytes_arriving_between_pumps > bytes_drained_per_pump  →  overflow
```

Do not do heavy work inside that callback. Queue complete NMEA lines and parse after the pump returns.

Empty `available()` while TX (commands) still works is an ESP32 RX-path stall or a module that stopped transmitting, not a CFGMSGRATE math error. Distinguish:

- `fifoOvf` / `bufferFull` climbing → UART driver not drained fast enough (call `drain` every loop)
- `drainOverruns` climbing → 8 kB ring lost oldest bytes; loop blocked elsewhere or ingress faster than frame+process
- `drainBudgetHits` climbing → 4 ms drain left bytes in the driver; next tick must catch up
- `rtcmDrops` climbing → RTCM FIFO (6 frames) overflow; NTRIP/process not keeping up
- `mailboxOverwrites` climbing → status replaced unread (expected for 1 Hz RMC if process is slow; not data loss of truth, latest-wins)
- `gsvDueCount` climbing → sky view skipped `gsvSkipLimit` groups (~40 s) and was promoted after RTCM+needed
- `available() == 0`, no overflow, no command replies → FIFO never filled (RX pin/driver, or module TX silent)

### How to stay inside the budget

1. **Lower RATE on bulky sentences** (`setMessageRate`). GSV at RATE 10 (0.1 Hz) cuts ~1 kB/s of NMEA. `$PQTMSVINSTATUS` does not need 1 Hz for a 12 h survey-in.
2. **Keep time/position sentences faster** (GGA/RMC at RATE 1) if the app needs 1 Hz time.
3. **Pump often.** Interval × ingress must fit in the RX buffer. At 3 kB/s ingress, a 200 ms gap is already ~600 bytes; a 1 s gap overflows a 1 kB buffer.
4. **Cap callback work.** 15 ms is a safety cap against stack/heap collapse, not a license to parse a full epoch on the UART stack.
5. **One reader.** Do not call `readLine` / `query*` from a second path while the bridge pump owns the Stream, or you steal/split frames.

## Module messages in practice

Quectel protocol PDFs list fields and ACKs. They do not say which sentences matter for a base vs a rover, what `RATE` really means, or which restart actually applies survey-in on LC29H(DA). This section is that usage note. The examples send these payloads through library helpers (`sendPayload` adds `$…*CS\r\n`).

Two directions on the same UART:

| Direction | What | Who cares |
| --- | --- | --- |
| **To the module** | `$PQTM…` / `$PAIR…` configuration | Sketch `setup()` / `LC29H_bringUp()` |
| **From the module** | NMEA text, `$PQTMSVINSTATUS`, RTCM frames (`0xD3`) | Sketch `loop()` pump or GIS app |

**Base mission:** RTCM MSM7 + 1005 at the nav epoch (1 Hz). NMEA is status.  
**Rover mission:** RTCM **in** (corrections), GGA (position) and RMC (time) **out**. Other NMEA is for GIS as needed.

### RATE is not Hertz

`$PQTMCFGMSGRATE,W,<MsgName>,<Rate>[,<MsgVer>]`

`<Rate>` is **every N navigation epochs**. `0` disables the sentence.

| Fix interval | RATE 1 | RATE 5 | RATE 10 | RATE 50 |
| --- | --- | --- | --- | --- |
| 1000 ms (base) | 1 s | 5 s | 10 s | 50 s |
| 200 ms (rover) | 200 ms | 1 s | 2 s | 10 s |

On LC29H(DA), **standard NMEA names omit `<MsgVer>`**. `$PQTM…` names still send it (`1`, or `2` for `PQTMEPE`). The library `setMessageRate()` does that. Hand-built payloads that always append `,1` after `GGA`/`GSV` can fail on DA.

### Commands to the module (what they actually do)

| Payload | Library helper | What happens | Field notes the PDF undersells |
| --- | --- | --- | --- |
| `PQTMCFGRCVRMODE,W,1` | `setReceiverModeRover()` | Rover (accepts RTCM, outputs position) | Base mode **stops** normal NMEA until you turn sentences back on. |
| `PQTMCFGRCVRMODE,W,2` | `setReceiverModeBase()` | Base (emits RTCM once enabled) | Same: restore GGA/RMC/GSV with CFGMSGRATE after this. |
| `PQTMCFGRCVRMODE,R` | `queryReceiverMode()` / `getReceiverMode()` | `OK,<mode>` 1=rover 2=base | Use this to see what you actually have after reboot. |
| `PQTMCFGSVIN,W,1,<MinDur>,<AccLimitM>,0,0,0` | `configureBaseSurveyIn()` | Start survey-in average | `<AccLimit>` is **meters**. Quectel default **15** starts `<Obs>` on DA. `0` is not usable; `1.5`/`2`/`8` left Obs at 0. `MinDur` is **fix count** (seconds at 1 Hz). ECEF must be 0,0,0 in mode 1. **Does not take effect** until SAVEPAR + **PAIR023**. |
| `PQTMCFGSVIN,R` | `querySurveyIn()` / `getSurveyInConfig()` | `OK,<Mode>,<MinDur>,<AccLimit>,X,Y,Z` | Mode 1 = survey-in, 2 = fixed ECEF. Matching Mode/MinDur/AccLimit means **adopt** the live run — do not write CFGSVIN or PAIR023 or you zero Obs. |
| `PQTMCFGSVIN,W,2,0,0,X,Y,Z` | `setFixedEcef()` | Lock ARP as fixed base | Use after Valid=2, not at survey start. |
| `PQTMSAVEPAR` | `saveConfig()` | Write working config to flash | Necessary but **not sufficient** for CFGSVIN on DA. |
| `PAIR023` | `rebootModule()` | Full **module** reboot | This is what makes CFGSVIN start counting Obs. UART goes silent for a few seconds. |
| `PAIR003` / `PAIR002` | (avoid for survey-in) | GNSS engine sleep / wake | **Not** a module reboot. Valid stayed 1 and Obs stayed 0 on DA. |
| `PQTMHOT` / `PQTMWARM` / `PQTMCOLD` | `hotStart()` / `warmStart()` / `coldStart()` | GNSS engine restart | Also not PAIR023. Fine for rover warm start; will not apply saved CFGSVIN the way PAIR023 does. |
| `PAIR432,1` | `enableRTCM(true)` | MSM7 observations | Base mission stream. Stays at the nav epoch (1 Hz). Do not slow this to “save UART”. |
| `PAIR434,1` | `enableRTCM(true)` | RTCM 1005 ARP | Goes with MSM7. Rovers need the ARP. |
| `PAIR432,-1` then `PAIR434,0` | `enableRTCM(false)` | Disable RTCM | **`PAIR432,0` is MSM4, not off.** |
| `PQTMCFGMSGRATE,W,<name>,<Rate>[,Ver]` | `setMessageRate()` | Sentence on/off/interval | See RATE table. GSV is the bulky NMEA item (many sentences per epoch). |
| `PQTMCFGFIXRATE,W,<ms>` | `setFixRateMs()` | Navigation epoch | Rover GIS: 200 ms = 5 Hz GGA/RMC. Base examples leave 1000 ms. RATE divisors scale with this. |
| `PQTMCFGFIXRATE,R` | `getFixRateMs()` | Read epoch | |
| `PQTMVERNO` | `queryVersion()` | Firmware string | Sanity that RX is alive. |
| `PQTMPVT` / `PQTMVEL` / `PQTMDOP` / `PQTMSTD` | rover profile, then often RATE 0 | Quectel binary-ish text diagnostics | GIS tools want NMEA GGA/RMC, not these. Rover examples **disable** them after the profile so UART stays on corrections + GIS sentences. |

### Sentences from the module

| Sentence | Typical use | Base schedule | Rover GIS schedule |
| --- | --- | --- | --- |
| `$--GGA` | Position (lat/lon/alt, fix quality) | RATE 1 (1 s) **status** | RATE 1 (every epoch) **mission out** |
| `$--RMC` | Time and date, course | RATE 1 (1 s) **status** | RATE 1 (every epoch) **mission out** |
| `$--VTG` | Speed / track | off unless you add it | RATE 1 (every epoch) for GIS |
| `$--GST` | Error ellipse / RMS | RATE 5 (5 s) | ~1 Hz (`RATE = 1000/fixMs`) |
| `$--GSA` | DOP, sats used | RATE 5 | ~1 Hz |
| `$--GSV` | Sats in view (many lines) | RATE 10 (10 s) | ~10 s (`RATE = 10000/fixMs`) |
| `$--ZDA` | UTC date/time | unused | ~1 Hz |
| `$PQTMEPE` | Estimated position error | RATE 5 | unused (GIS uses GST) |
| `$PQTMSVINSTATUS` | Survey-in engine | RATE 10 | unused |
| RTCM MSM7 (`1077`/`1087`/`1097`/`1127`…) | Observations | 1 Hz **mission** | **input** (write raw to GNSS) |
| RTCM 1005 | Base ARP | 1 Hz **mission** | **input** |

`$PQTMSVINSTATUS` fields used in practice (after MsgVer, TOW): **Valid**, **Obs**, **CfgDur**, MeanX/Y/Z, **MeanAcc**.

| Valid | Meaning |
| --- | --- |
| 0 | Idle / no engine |
| 1 | Survey-in running (watch Obs increase) |
| 2 | Survey-in complete (then you may `survey_finalize`) |

MeanAcc **`0.0000` is a placeholder until Obs > 0**. It is not “zero error” and must not complete a survey.

### Which example sends or needs which

`LC29H_bringUp()` is the common path. Reduced sketches send the same payloads by hand.

| Example | Role | Commands **to** GNSS | Bytes **from** GNSS it cares about |
| --- | --- | --- | --- |
| SimpleBaseStation | Base survey | CFGSVIN (or adopt via `PQTMCFGSVIN,R`), SAVEPAR, PAIR023, PAIR432/434, base CFGMSGRATE table | `$PQTMSVINSTATUS`, GGA/RMC status; RTCM is generated but discarded (no second port) |
| ESP32BaseStation | Base survey | Same | RTCM **out Serial2** (mission); NMEA optional on USB |
| StreamBridge | Base survey | Same | RTCM (+ allowlisted NMEA) **out link UART** |
| BaseSerialBridge | Base survey | Same | RTCM **out to rover UART** |
| BasicConfiguration | Base survey (default) | Same, plus whatever you type (`help`, `msg_on`, `reboot`, …) | Printed NMEA / PAIR ACKs |
| SimpleRover | Rover | CFGRCVRMODE rover, CFGFIXRATE, GIS CFGMSGRATE, SAVEPAR, PAIR023 if needed | **RTCM in** (ESP32 Serial2), **GGA+RMC out** USB |
| RoverCorrectionBridge | Rover | Same | **RTCM in** from link, **GGA+RMC (+GST)** out |
| ESP32BtRoamer | Rover | Same | **RTCM in** from phone, **GGA+RMC out** to phone |
| ESP32UsbUartBridge | Passthrough | None by default (QGNSS owns config). Optional `LC29H_bringUp()` | Raw both ways on CH340 |
| ReducedCommandConsole | Manual | You type `PQTMCFGSVIN…`, `PQTMSAVEPAR`, `PAIR023`, `PQTMCFGMSGRATE…` | Raw copy to USB |
| ReducedSerialBridge | Passthrough | None | Raw USB ↔ GNSS |

Do not PAIR023 over a live survey-in whose MinDur/AccLimit already match. That is the adopt path in `LC29H_bringUp()`.

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
- LC29H_MessageSchedule.h
- examples/README.md (index; each sketch folder also has README.md)
- examples (see that index for how each sketch uses the library)
- CommandReference.md

Note: this repository currently keeps both source files and metadata/docs in the root directory.

## Quick start

1. Copy/open this repository in your Arduino libraries folder.
2. Open the example-local lc29hconfig.h in the example directory you want to run.
3. Adjust role and values in that lc29hconfig.h for your project type (UAS rover, base survey, or static base).
4. Open examples/README.md, then the example folder you want (start with SimpleBaseStation or SimpleRover).
5. Set GNSS serial pins/port for your board.
6. Open Serial Monitor at 115200 baud.
7. Type help and use interactive commands.

Example config behavior:

- All examples include LC29H_ProjectConfig.h and require lc29hconfig.h.
- Each example directory includes its own lc29hconfig.h; edit that local file before running the sketch.
- The examples print a clear message and stay disabled until that file is provided.

## Minimal examples

Each sketch folder has a README that matches the comments at the top of the `.ino`. Index: examples/README.md.

- SimpleRover: ingest RTCM (ESP32 Serial2) first, publish GGA+RMC every epoch for GIS.
- SimpleBaseStation: survey-in base; adopt a live matching SVIN or SAVEPAR+PAIR023; drain UART; RTCM 1 Hz.
- BasicConfiguration: same bring-up plus Serial Monitor `help` commands.
- BaseSerialBridge / RoverCorrectionBridge: wired bench pair (RTCM base → rover; rover GGA+RMC out).
- ESP32BaseStation: ESP32 Serial1 GNSS, Serial2 RTCM out.
- ESP32UsbUartBridge: native USB console + CH340 raw GNSS for QGNSS (one host at a time).
- ESP32BtRoamer: phone Bluetooth RTCM in, GGA+RMC out (SPP or BLE).

## Reduced capability examples (UNO/Micro/Feather class)

- ReducedCommandConsole: typed payloads, `save`/`reboot` (PAIR023), no LC29H_GNSS object.
- ReducedSerialBridge: raw USB Serial <-> GNSS UART, one reader each side.

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
  - Pump every loop; do not parse inside the pump callback. RTCM at 1 Hz is the mission stream.
  - Bridge mode and NMEA allowlist are configurable from lc29hconfig.h.

## ESP32 base station example

- ESP32BaseStation: examples/ESP32BaseStation/ESP32BaseStation.ino
  - ESP32-only example with GNSS on Serial1 and RTCM/link output on Serial2.
  - Intended for ESP32-S3-DevKitC-1 compatible boards (including common clone variants).
  - UART GPIOs are defined in the example-local lc29hconfig.h, which is the source of truth for clone-specific pin routing.
  - The default example avoids GPIO19/GPIO20 because those are commonly used by ESP32-S3 native USB.
  - Uses the same project config flow as the generic examples, but removes non-ESP32 fallback code.
  - After the survey profile: GSV/SVIN RATE 10, SAVEPAR, PAIR023.
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

- v0.2.1

Fixes in v0.2.1:

1. Removed a stray block accidentally inserted into the accuracy-tracker configuration path.
2. Corrected command-family metadata defaults so field arrays and response metadata use the proper internal types.
3. Reduced SRAM usage for the full-size AVR Mega examples by disabling the interactive command loop on Mega targets only.

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
7. A separate GitHub Actions expected-failure job tracks those Uno/Nano SRAM-limit cases so changes to that behavior remain visible without breaking the main validation lane.

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

The `help` command now works in two layers:

- `help` prints grouped command families.
- `help <command>` prints the syntax and purpose for that command.
- `help families` or `help groups` prints the family taxonomy used by the library.
- `help metadata <command>` prints the metadata schema for a command family or specific command.
- `help family <name>` prints the default schema for a command family.
- `help registry` prints the verified, provisional, and placeholder command inventory.
- `help placeholders` prints the reserved V1.5 TODO slots and their policy.
- `help bridge` prints the bridge-mode summary and NMEA allowlist defaults.

Legend:

- REQUIRED_ARG = required argument
- [arg] = optional argument
- 0or1 = boolean flag (0=false, 1=true)

General and setup:

- help [command]
  - Syntax: help or help COMMAND_NAME
  - Purpose: print command list or detailed help for one command.
  - Example: help base_survey
- families
  - Syntax: families
  - Purpose: print the command-family summary used by the library.
  - Example: families
- status
  - Syntax: status
  - Purpose: run a quick health/query bundle (version, mode, survey status, baud query).
- restore
  - Syntax: restore
  - Purpose: restore receiver defaults.
- save
  - Syntax: save
  - Purpose: save current configuration to receiver flash (PQTMSAVEPAR).
- reboot
  - Syntax: reboot
  - Purpose: full module reboot (PAIR023). Required on DA/EA after SAVEPAR for survey-in. PAIR003/PAIR002 sleep is not a reboot.

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
  - Purpose: configure Survey-In base mode. AccLimit is meters; default 15 starts <Obs> on DA. Then save and reboot.
  - Example: base_survey 300 15
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
  - Example: profile_base_survey 3600 15 1 1 1
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
  - Purpose: capture Survey-In ECEF and apply it as fixed-base in one flow. Use only after `$PQTMSVINSTATUS` Valid=2, not at survey start.
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

Family summary:

- Identity: qver, uid
- Identity extras: serial number query via `querySerialNumber()`
- Lifecycle: restore, save, reboot, hot, warm, cold, gnss_start, gnss_stop
- Core configuration: rover, base, base_survey, base_fixed, mode_query, msg_on, msg_off, msg_query, baud, baud_query, fixrate, fixrate_query, rtcm
- Core configuration extras: PPS config via `setPulsePerSecondConfig()` and `queryPulsePerSecondConfig()`
- Survey and base workflows: profile_uas, profile_base_survey, profile_base_static, survey_capture, survey_pos, survey_apply, survey_finalize
- Output and diagnostics: status, survey_status, rover_status
- Transport and bridge: send
- Pair control: PAIR helpers plus `readPairAck()` and `rebootModule()` (PAIR023)

## Command metadata schema

The library now carries protocol metadata so the remaining command families can be added without inventing a new parsing shape for every wrapper.

Metadata fields tracked per command:

- base command
- wrapper name
- command family
- direction: write, read, read/write, or control
- ACK kind: none, pair ACK, command OK/error, status line, or direct data
- summary
- firmware or module gates
- save recommended
- power-cycle recommended
- generic fallback allowed
- request field specs
- response field specs
- response prefix and notes

The practical rule is simple: one family defines the default schema, and individual commands override only the fields that differ.

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
  - Caps NMEA assembly at kMaxBridgeNmeaChars and aborts on non-ASCII so mixed RTCM cannot grow
    an unbounded String (see Version 0.2.3). Resync counts are in BridgeStats::nmeaLineResyncs.
  - Returns after kMaxBridgePumpMs even when maxBytes is 0 (see Version 0.2.4).
  - Skips the built-in accuracy tracker when localNmeaOut is provided.
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
LC29H_GNSS::PresetResult r = gnss.applyBaseSurveyPreset(3600, 15.0f, true, true);
if (r == LC29H_GNSS::PresetResult::Success && gnss.isPowerCycleRecommended()) {
  gnss.rebootModule();  // PAIR023; PAIR003/PAIR002 sleep is not enough
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
