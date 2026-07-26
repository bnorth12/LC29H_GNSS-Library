# LC29H Command Reference (Observed)

This file captures commands observed in QGNSS logs/config and wired into the initial library.

Known limit:

- PQTMCFGBASE is log-backed here and remains provisional until module-specific ACKs confirm it on your target firmware.

## Observed from Temp/QGNSS files

- PQTMVERNO
  - Purpose: Query module firmware/variant.
  - Example: $PQTMVERNO*58

- PQTMRESTOREPAR
  - Purpose: Restore configuration defaults.
  - Example in logs appears without checksum while typing.

- PQTMCFGRCVRMODE,W,1
  - Purpose: Set receiver mode to rover.

- PQTMCFGRCVRMODE,W,2
  - Purpose: Set receiver mode to base.

- PQTMCFGSVIN,W,1,TIME_SEC,STDDEV_TENTH_M,0,0,0
  - Purpose: Configure Survey-In.
  - Observed examples:
    - $PQTMCFGSVIN,W,1,3600,15,0,0,0*1
    - $PQTMCFGSVIN,W,1,84600,15,0,0,0*1C

- PQTMCFGSVIN,R
  - Purpose: Read Survey-In config/status.

- PQTMCFGMSGRATE,W,MESSAGE_NAME,PORT,RATE
  - Purpose: Configure message output rate.
  - Observed example:
    - $PQTMCFGMSGRATE,W,PQTMSVINSTATUS,1,1*58

- PQTMCFGFIXRATE,W,MS
  - Purpose: Configure rover/fix output interval.

- PQTMCFGBASE,1,LAT_DDMM_MMMM,NS,LON_DDDMM_MMMM,EW,ALT_M
  - Purpose: Configure base fixed position.
  - Observed example:
    - $PQTMCFGBASE,1,3315.5960,N,09753.8202,W,276*2E
  - Note: this command is currently documented from QGNSS logs and remains provisional until confirmed against module-specific ACKs.

- PAIR432,1 and PAIR434,1
  - Purpose: Used in QGNSS while enabling RTCM-related behavior.

- PAIR432,0 and PAIR434,0
  - Purpose: Used by this library to disable the same behavior.

## Also present in QGNSS ModeInfo.json

- PAIR004, PAIR005, PAIR006, PAIR007, PAIR023
  - These appear as hot/warm/cold/full/restart commands in tool config and can be exposed in later API passes.

## Important note

For commands not yet fully validated against protocol PDFs, keep usage in bench-test mode and verify receiver responses (for example with PAIR001 ACK and status lines).

## Temp deprecation note

This reference intentionally copies the command seed set into the repository root so the Temp directory can be removed after initial development without losing command provenance.
