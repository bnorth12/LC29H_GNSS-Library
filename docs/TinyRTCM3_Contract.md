# TinyRTCM3 independence contract

LC29H_GNSS and TinyRTCM3 are **fully independent libraries**. Neither depends on the
other in either direction.

- No LC29H_GNSS source or header includes `<TinyRTCM3.h>`, uses `__has_include` to detect
  it, or changes class layout based on its presence (removed in 0.3.2; the earlier
  `LC29H_HAS_TINYRTCM3` gate risked ODR / sizeof mismatches between translation units).
- `library.properties` has no `depends=` on TinyRTCM3.
- There is no stream-stats hook on `LC29H_UartPump` or `LC29H_Rtcm::Assembler` (the old
  TinyRTCM3 stats hook was removed in 0.3.2; do not look for it). The pump frames
  RTCM and checks CRC-24Q itself and exposes whole frames plus `PumpHealth`.
- LC29H_GNSS examples use only LC29H_GNSS. Any sketch that needs both libraries lives in
  the application repo (for example esp32s3-lc29h-base-station), which wires LC29H output
  frames into TinyRTCM3.
- CI enforces this with an independence check that fails on any TinyRTCM3 reference in
  sources or examples.

Normative RTCM docs live in the TinyRTCM3 repo:

- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/ARCHITECTURE.md
- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/INTEGRATION.md
