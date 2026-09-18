# Gated / mode-specific outputs (LC29H BA/DA/EA/BS)

Additional **configuration details** for messages that only appear (or only carry
useful content) when the module is in a matching mode. Complements CommandReference.md.

Companion capture plan: TinyRTCM3 `docs/GATED_CAPTURE_MODES.md`.

## Important DA/EA constraint (PAIR062)

On **DA/EA**, `PAIR062` / `PAIR063` NMEA type indices **0–5 only** (not ZDA/GRS/GST/GNS).
For ZDA, GRS, GST, GNS, GST-like quality, SVIN status, EPE, etc., use:

```text
$PQTMCFGMSGRATE,W,<MsgName>,<Rate>[,<MsgVer>]
```

`setMessageRate()` / `enableMessageOutput()` already follow that rule. Prefer them
over `setPairNmeaOutputRate()` for those sentences.

## Mode recipes

### 1. Survey-in status — `PQTMSVINSTATUS`

**Need:** base receiver + survey-in running.

```text
PQTMCFGRCVRMODE,W,2
PQTMCFGSVIN,W,1,<MinDur>,<AccLimit_m>,0,0,0   # AccLimit e.g. 15
PQTMSAVEPAR
PAIR023
PQTMCFGMSGRATE,W,PQTMSVINSTATUS,1,1
```

Library: `configureBaseSurveyIn` / `applySurveyBaseProfile`, then ensure SVINSTATUS rate 1.
Parse with `tryParseSvinStatus`. **Do not PAIR023** again if adopting a live matching SVIN
(`getSurveyInConfig`) or you zero `<Obs>`.

### 2. Geofence status — `PQTMGEOFENCESTATUS`

**Need:** geofence configured (`PQTMCFGGEOFENCE` family) + status rate 1.
Library today exposes `queryGeoFenceStatus()`; full fence write helpers may still be thin —
document commands here as configs are added.

### 3. Jamming status — `PQTMJAMMINGSTATUS`

**Need:** AIC / jamming detect enabled (`PQTMCFGAIC` and/or `PAIR074`), then:

```text
PQTMCFGMSGRATE,W,PQTMJAMMINGSTATUS,1,1
```

Library: `queryJammingStatus()` for polls; continuous output needs CFGMSGRATE.

### 4. ZDA / GRS / GST / GNS

Enable via **CFGMSGRATE only** on DA/EA after a valid fix (and UTC for ZDA).
GST/GRS may need a healthy nav solution; treat EMPTY without fix+rate as inconclusive.

### 5. RTCM ephemeris output — PAIR436

```text
PQTMCFGRCVRMODE,W,2
PAIR432,-1          # MSM off (optional clarity)
PAIR434,0           # 1005 off optional
PAIR436,1           # ephemeris RTCM on
PQTMSAVEPAR
PAIR023
```

Query: `PAIR437`. Verify CRC-valid 1019/1020/1042/1044/1046 before claiming success.
MSM+1005 mission stream uses `enableRTCM()` (PAIR432/434), not PAIR436.

### 6. Not available as LC29H TX

| Type | Protocol | Implication |
|------|----------|-------------|
| RTCM **1006** | Input only | Rover can consume; base TX goldens = synthetic |
| RTCM **1033** | Not in LC29H RTCM table | Synthetic / host encode for NTRIP/DePIN |

## Save / reboot reminder

`PQTMSAVEPAR` is not a reboot. On **DA/EA**, CFGSVIN and CFGRCVRMODE changes need **`PAIR023`**
to take effect. GNSS sleep (PAIR003/002) is not enough for those.
