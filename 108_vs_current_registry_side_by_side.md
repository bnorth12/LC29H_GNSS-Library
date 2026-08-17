# 108 vs Current Registry Side-by-Side

This document compares the V1.5 protocol evaluation count with the command bases recoverable from the checked-in repository references.

## Summary

- V1.5 protocol evaluation: 108 unique command bases
- Checked-in repository references: 40 normalized command bases
- Additional helper rows in the registry: 1 transport helper row
- Unresolved V1.5 slots: 68

## Side-by-side by family

| Family | Recovered from repo references | Registry status | Best source for missing/additional data |
| --- | --- | --- | --- |
| Identity | PQTMVERNO, PQTMQVER, PQTMUNIQID, PQTMSN | Fully recovered from repo references | Canonical normalized command-bases file; live module responses for identity fields |
| Lifecycle | PQTMRESTOREPAR, PQTMSAVEPAR, PQTMHOT, PQTMWARM, PQTMCOLD, PQTMGNSSSTART, PQTMGNSSSTOP | Fully recovered from repo references | Canonical normalized command-bases file; command/ACK examples from logs |
| Core configuration | PQTMCFGRCVRMODE, PQTMCFGMSGRATE, PQTMCFGFIXRATE, PQTMCFGUART, PQTMCFGPROT, PQTMCFGCNST, PQTMCFGNAVMODE, PQTMCFGNMEADP, PQTMCFGNMEATID, PQTMCFGPPS | Fully recovered from repo references | Canonical normalized command-bases file; protocol PDF field tables; live ACK validation for argument shapes |
| Survey and base | PQTMCFGSVIN, PQTMCFGBASE (provisional) | Partially recovered | Module-specific protocol docs; live ACK traces; QGNSS logs for fixed-base behavior |
| Output and diagnostics | PQTMPVT, PQTMVEL, PQTMSTD, PQTMDOP, PQTMJAMMINGSTATUS, PQTMGEOFENCESTATUS, PQTMODO, PQTMSVINSTATUS, PQTMMEPE | Mostly recovered, with PQTMMEPE provisional | Protocol PDF extraction; QGNSS logs; live data lines for exact response fields |
| Pair control | PAIR001, PAIR004, PAIR005, PAIR006, PAIR007, PAIR023, PAIR432, PAIR434 | Partially recovered | ModeInfo.json; QGNSS logs; live PAIR001 ACKs; protocol spec for command families |
| Transport and bridge | raw ingress/egress helpers only | Helper coverage, not protocol-base coverage | Protocol PDF for any bridge-related command bases; application-specific bridge traces |

## What the repo can already tell us

The current checked-in references are enough to build and validate the following:

- command-family taxonomy
- metadata schema for direction, ACK kind, field specs, and response specs
- family-default metadata
- 40 recovered command bases plus 1 helper row
- help output for both command-level and family-level inspection

## Where additional information can come from

Priority order for filling the remaining 68 slots:

1. Canonical normalized command-bases file from the Temp protocol extraction
2. The original V1.5 protocol PDF extraction table
3. QGNSS ModeInfo.json for PAIR and startup-related command families
4. QGNSS log files for observed payload shapes and provisional behaviors
5. Live module ACKs and serial traces to confirm firmware-specific fields and save/restart behavior

## Interpretation rule

Do not infer the remaining 68 command bases from the recovered 40 alone. Use the registry and the protocol sources together:

- recover from repo references when the command already appears in the checked-in docs/code
- confirm from protocol docs when the field layout is still uncertain
- validate from live hardware when the command is firmware-specific or provisional
