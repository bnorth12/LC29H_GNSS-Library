# TinyRTCM3 contract (pointer)

## Hard rule: optional peer

**TinyRTCM3 is optional.** LC29H_GNSS must compile, link, and run its public API and
examples **without** TinyRTCM3 on the include path or in package manifests.

| May | Must not |
|-----|----------|
| Apps use LC29H alone, TinyRTCM3 alone, or both | List TinyRTCM3 as a required dependency of this library |
| Ship a separate opt-in example that `#include`s TinyRTCM3 | `#include` TinyRTCM3 from required public headers |
| Document Hub as a recommended RTCM pipe | Break existing sketches when TinyRTCM3 is absent |

## Ownership split

LC29H_GNSS owns **Quectel config + UART pump / NMEA**. RTCM 3 framing, CRC-24Q,
type identification, filter/emit, and selective codecs live in **TinyRTCM3**.

Normative docs in the TinyRTCM3 repo:

- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/ARCHITECTURE.md
- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/INTEGRATION.md

Do not add a second RTCM assembler here during TinyRTCM3 Phase A-B.
Optional side-by-side consume is Phase B; any pump cutover is an **application**
choice (Phase C), not a forced dependency of this library package.
