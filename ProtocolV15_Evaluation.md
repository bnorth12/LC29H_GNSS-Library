# Protocol Evaluation: Quectel_LC29HLC79H_Series_GNSS_Protocol_Specification_V1.5

Date: 2026-07-26
Source evaluated: Temp/Quectel GPS Documents/Quectel_LC29HLC79H_Series_GNSS_Protocol_Specification_V1.5.pdf

## Extraction result

- PDF pages parsed: 147
- Extracted text chars: 186,721
- Raw command-like lines found ($PQTM/$PAIR): 435
- Unique command bases (normalized): 108

Normalized command list used for this evaluation is in:

- Temp/lc29h_protocol_v1_5_command_bases.txt

## Library coverage status (current implementation)

Current wrapper methods in the library directly cover these protocol commands:

- PQTMVERNO
- PQTMRESTOREPAR
- PQTMSAVEPAR
- PQTMCFGSVIN
- PQTMCFGRCVRMODE
- PQTMCFGMSGRATE
- PQTMCFGFIXRATE
- PAIR432
- PAIR434

Plus generic command transport support:

- sendCommand(command, args)
- sendPayload(payload)
- makeSentence(payload) checksum builder

This means most protocol commands are currently sendable through generic APIs even if not wrapped by dedicated methods.

## Coverage findings

- Unique command bases in V1.5 spec: 108
- Dedicated wrappers implemented for bases in V1.5 spec: 9
- Remaining command bases without dedicated wrappers: 100

Approximate dedicated-wrapper coverage ratio:

- 9 / 108 = 8.3%

## Important mismatch found

The code currently uses PQTMCFGBASE for fixed-base setup, but this command was not found in the extracted base list from V1.5.

Possible reasons:

- Command appears in another Quectel document (application note / module-specific protocol)
- Command naming variant exists in this PDF but was missed by extraction formatting
- Command is firmware/module-specific and not in this series-level spec revision

Recommendation:

- Treat PQTMCFGBASE as provisional until confirmed against module-specific docs and live ACK responses.

## Command families present in V1.5 spec

Examples from extracted list:

- Reset/startup: PQTMHOT, PQTMWARM, PQTMCOLD, PQTMGNSSSTART, PQTMGNSSSTOP
- Receiver/config: PQTMCFGRCVRMODE, PQTMCFGSVIN, PQTMCFGMSGRATE, PQTMCFGUART, PQTMCFGPROT, PQTMCFGCNST, PQTMCFGNMEADP, PQTMCFGNMEATID, PQTMCFGPPS
- Status/output: PQTMPVT, PQTMVEL, PQTMSTD, PQTMDOP, PQtMEPE, PQTMJAMMINGSTATUS, PQTMGEOFENCESTATUS, PQTMSVINSTATUS
- Identity/version: PQTMVERNO, PQTMQVER, PQTMUNIQID, PQTMSN
- PAIR controls: PAIR004..PAIR007 and many PAIR0xx/4xx/8xx commands

## Suggested next implementation pass

1. Add typed wrappers for high-impact configuration commands first:
   - PQTMCFGUART
   - PQTMCFGPROT
   - PQTMCFGCNST
   - PQTMCFGNMEADP
2. Add standardized ACK/ERR parsing for:
   - PAIR001
   - COMMAND,OK
   - COMMAND,ERROR
3. Build a command-to-method traceability table in root for Temp-independent maintenance.

## Current implementation notes

- configureRover(outputMs) now applies rover mode and PQTMCFGFIXRATE directly.
- Fixed-base support is still implemented through the observed PQTMCFGBASE command family from QGNSS logs, so treat that path as log-backed rather than V1.5-spec-confirmed.

## Known limits

- The V1.5 PDF evaluation is still a coverage map, not a full ACK-validated behavior matrix.
- Dedicated wrappers intentionally focus on the most important setup and bridge flows first.
- Commands outside the wrapper set remain reachable through generic payload/sentence helpers, but they are not yet individually documented or typed here.

## Bottom line

No, the library does not yet cover all commands listed in the V1.5 protocol spec with dedicated methods.

It currently provides:

- Strong generic command sending with automatic checksum
- A starter set of dedicated wrappers for core rover/base setup flows
