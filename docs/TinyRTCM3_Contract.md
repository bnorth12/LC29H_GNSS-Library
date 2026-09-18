# TinyRTCM3 contract (pointer)

LC29H_GNSS owns **Quectel config + UART pump / NMEA**. RTCM 3 framing, CRC-24Q,
type identification, filter/emit, and selective codecs live in **TinyRTCM3**.

Normative ownership and phased integration (when this library may start calling
`tinyrtcm3::Hub`) are locked in the TinyRTCM3 repo:

- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/ARCHITECTURE.md
- https://github.com/bnorth12/TinyRTCM3/blob/main/docs/INTEGRATION.md

**Do not** add a second RTCM assembler here during TinyRTCM3 Phase A-B.
Optional side-by-side consume is Phase B; pump cutover is Phase C.
