# Issue #3 / PR #4 — survey-adopt receiver mode (B2)

Survey-config match (`PQTMCFGSVIN` mode=1 + MinDur + AccLimit) is necessary but not sufficient.

`LC29H_bringUp` (BASE_SURVEY) now calls `getReceiverMode` and only adopts when **receiver mode == 2 (base)**.
Matching survey config while still rover (mode 1) refuses adopt and falls through to `applyProjectConfig`.

`StreamBridge` READY is gated the same way: QUERY_FAIL or mode != 2 => STATUS=FAIL (not READY).
