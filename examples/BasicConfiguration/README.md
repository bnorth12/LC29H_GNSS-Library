# BasicConfiguration

Interactive reference on the same bring-up as SimpleBaseStation. After `LC29H_bringUp()`, it queries version/mode/baud and leaves the Serial Monitor command processor running (`help`, `reboot`, `base_survey`, `survey_finalize`, …).

## What it does

- Default `lc29hconfig.h` role is survey-base (AccLimit 15 m, finalize off).
- Adopts a matching live survey-in, or starts a new one with SAVEPAR + PAIR023.
- Status NMEA is scheduled; RTCM stays 1 Hz if the role is a base.
- `survey_finalize` only after `$PQTMSVINSTATUS` Valid=2. Typed console queries share the GNSS UART with the drain — use them between bursts.

## Hardware

Mega-class AVR or ESP32 Serial1 RX16/TX17.

## Config

This folder’s `lc29hconfig.h`. Switch `LC29H_ROLE` to try rover or static-base profiles through the same sketch.

## Messages this sketch uses

Same bring-up as SimpleBaseStation (or rover/static if you change `LC29H_ROLE`). See [Module messages in practice](../../Readme.md#module-messages-in-practice).

Serial Monitor `help` can send extra payloads (`base_survey`, `msg_on`, `reboot` = PAIR023, `survey_finalize` only after Valid=2). Those share the GNSS UART with the drain.
