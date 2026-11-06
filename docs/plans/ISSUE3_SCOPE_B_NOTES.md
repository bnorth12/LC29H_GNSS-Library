# Issue #3 Scope B - SimpleRover reply-loss notes

**Conclusion (2026-11-05 CT, updated after Reviewer B3): library change still NOT applied; VERIFY=1 discriminating case FAILED — escalate to RTK before any .cpp change.**

## Evidence
1. Wrong GNSS pins (hardcoded RX16/TX17) vs board **RX17/TX18** — fixed via `lc29h_board.h`.
2. `HostPump::beginGnss` set `setTimeout(0)` **before** bring-up — deferred until after `LC29H_bringUp`.
3. `LC29H_CFG_VERIFY=0` under live NMEA is the pragmatic example default (template note: NMEA starves PQTM readback).
4. Pre-bring-up UART settle/drain reduced `Module family=unknown` on first sketches after BS rates.
5. **SimpleRover VERIFY=0** HIL: `STATUS=READY` with example-only fixes (earlier 11/11 pass set).
6. **SimpleRover VERIFY=1 + deferred setTimeout(0), clean pins RX17/TX18, 10 Hz / 115200 (2026-11-05 ~3:50 PM CT):**
   - Module family=EA
   - Commands TX OK through SAVEPAR
   - Bring-up **VerifyFailed** (CFGRCVRMODE/FIXRATE readback)
   - `HIL_EXAMPLE=SimpleRover STATUS=FAIL bringup=VerifyFailed`
   - Capture: `%TEMP%\lc29h_i3_hil\verify1\SimpleRover_VERIFY1.serial.txt` and box `/workspace/lc29h-i3-hil/verify1/`

## Escalation (no library widen in this PR)
Per issue #3 / Reviewer B3: do **not** change library reply-matching until RTK decides. Example defaults remain VERIFY=0 + deferred timeout.
