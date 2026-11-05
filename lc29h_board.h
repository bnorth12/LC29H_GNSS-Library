#pragma once

// Default ESP32 pin map for the LC29H Base Station / N16R8-class board used in HIL.
// Override any symbol with -D (e.g. -DLC29H_BOARD_GNSS_RX_PIN=18) or before including.

#ifndef LC29H_BOARD_GNSS_RX_PIN
#define LC29H_BOARD_GNSS_RX_PIN 17
#endif
#ifndef LC29H_BOARD_GNSS_TX_PIN
#define LC29H_BOARD_GNSS_TX_PIN 18
#endif
#ifndef LC29H_BOARD_GNSS_BAUD
#define LC29H_BOARD_GNSS_BAUD 115200
#endif
#ifndef LC29H_BOARD_CORR_RX_PIN
#define LC29H_BOARD_CORR_RX_PIN 5
#endif
#ifndef LC29H_BOARD_CORR_TX_PIN
#define LC29H_BOARD_CORR_TX_PIN 4
#endif
#ifndef LC29H_BOARD_LINK_RX_PIN
#define LC29H_BOARD_LINK_RX_PIN 8
#endif
#ifndef LC29H_BOARD_LINK_TX_PIN
#define LC29H_BOARD_LINK_TX_PIN 9
#endif
