# Issue #3 Scope B — SimpleRover reply-loss notes

**Conclusion (2026-11-05 CT): library change NOT required for this round.**

## Evidence
1. Wrong GNSS pins (hardcoded RX16/TX17) vs board **RX17/TX18** — fixed via `lc29h_board.h`.
2. `HostPump::beginGnss` set `setTimeout(0)` **before** bring-up — deferred until after `LC29H_bringUp`.
3. `LC29H_CFG_VERIFY=1` under live NMEA → VerifyFailed even when writes OK (template note). HIL configs use VERIFY=0.
4. Pre-bring-up UART settle/drain reduced `Module family=unknown` on first sketches after BS rates.
5. **SimpleRover** HIL: `HIL_EXAMPLE=SimpleRover STATUS=READY` with example-only fixes (no library `.cpp` change). Capture: `%TEMP%\lc29h_i3_hil\SimpleRover.serial.txt`.

## Still library-adjacent (do not widen this PR)
- PQTM reply matching under zero read-timeout + 10 Hz NMEA remains fragile if examples call `setTimeout(0)` too early or leave VERIFY=1.
- ESP32UsbUartBridge UART1 baud query still soft-fails intermittently (HIL READY when ≥2/3 core checks pass).
- EA GST still 0 in Fixed soak (product/module, not example).

Escalate library reply-matching only if SimpleRover VERIFY=1 + deferred timeout still VerifyFailed on clean pins.
