# LC29H 108-Command Integration Backlog

Status key:

- Done: already implemented in the library
- In progress: scaffolded or partially implemented
- Next: ready to execute after the current slice
- Blocked: needs canonical protocol data or live module validation

## Phase 0: Registry and schema

- Done: command-family taxonomy in the public API
- Done: metadata schema types for direction, ACK kind, field specs, and response specs
- Done: `help` family overview and metadata lookup helpers
- Done: family defaults for request and response field templates
- Done: normalized registry scaffold created from checked-in references (`CommandRegistry.md`)
- Done: normalized command-base list extracted from checked-in references (`NormalizedCommandBases_fromRefs.txt`)
- Blocked: canonical 108-command registry source checked into the repo

## Phase 1: Core families

- Done: identity queries for version, Q-version, unique ID, serial number
- Done: lifecycle control for restore, save, GNSS start/stop, hot/warm/cold restart
- Done: core config wrappers for rover/base mode, fix rate, message rate, baud, protocol mask, constellations, NMEA formatting, PPS
- Done: survey/base wrappers for Survey-In, fixed base, and ECEF workflows
- Next: add any additional identity or lifecycle entries only when the canonical Temp extraction confirms them
- Next: add remaining core configuration entries only after field shapes are confirmed from protocol text or live validation
- Next: keep survey/base family additions provisional until target-firmware ACKs confirm the fixed-base path

## Phase 1b: Family-by-family TODO matrix

Use this matrix to place the remaining 68 V1.5 placeholders without inventing new identities.

| Family | Current state | Placeholder rule | Source priority |
| --- | --- | --- | --- |
| Identity | Fully covered by verified wrappers | No placeholders expected unless canonical Temp extraction adds a new identity command | Canonical Temp extraction, then live module response if needed |
| Lifecycle | Fully covered by verified wrappers | No placeholders expected unless canonical Temp extraction adds a new reset/startup variant | Canonical Temp extraction, then module ACK logs |
| Core configuration | Broadly covered; additional commands remain possible | Add TODO slots only after command name and field layout are recovered together | Canonical Temp extraction, protocol text, then live validation |
| Survey and base | One provisional fixed-base path plus Survey-In coverage | Keep fixed-base as provisional; add new TODO rows only with module-specific confirmation | Module-specific ACKs, QGNSS logs, then protocol text |
| Output and diagnostics | Most high-use queries covered; PQTMMEPE remains provisional | Reserve extra status/query slots as placeholders until exact prefixes and payload fields are known | Protocol text, QGNSS logs, then live telemetry |
| Pair control | PAIR001 plus observed ModeInfo-backed family members are known | Add TODO slots as family placeholders only; do not guess command numbering | ModeInfo.json, PAIR001 ACKs, then protocol text |
| Transport and bridge | Helper-only, not a protocol-base family | Do not spend V1.5 placeholder slots here unless the protocol text proves a real command base | Library behavior, then protocol text if a real base appears |

Current known/provisional inventory for reference:

- Verified command bases: 38
- Provisional command bases: 2
- Transport helper row: 1
- TODO placeholders: 68

That inventory should remain stable until the canonical 108-base list is checked in.

## Phase 2: Output and diagnostics

- Done: wrappers for PVT, velocity, standard deviation, DOP, jamming, geofence, and odometer query paths
- Next: add any missing output/status commands from the canonical list, but only as family-level placeholders until the command base is recovered
- Next: add status-line response parsers keyed by metadata

## Phase 3: Pair control

- Done: PAIR001 ACK parsing and wait helper
- Next: registry entries for the remaining PAIR command bases as TODO placeholders, not guessed names
- Blocked: exact request/response schemas for the rest of the PAIR family

## Phase 4: Transport and bridge

- Done: raw ingress/egress helpers and bridge forwarding helpers
- Next: family metadata entries for bridge-related command bases
- Next: family-level docs for bridge modes and NMEA allowlists

## Phase 5: Validation and rollout

- Next: compile validation after each family slice
- Next: bench ACK validation for each added wrapper
- Next: mark each registry entry as validated, provisional, or generic-fallback only
- Next: generate command help and traceability output from the registry

## Execution rule

Add commands family-by-family, not command-by-command in isolation, unless a command is a known outlier. That keeps the field templates reusable and prevents the 108 from turning into 108 unrelated APIs.
