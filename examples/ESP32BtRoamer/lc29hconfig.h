#pragma once

// Example-local project config for ESP32BtRoamer.
// This rover ingests RTCM corrections over Bluetooth:
// - Classic ESP32 target: BT Classic SPP
// - ESP32-S3 target: BLE GATT ingress

// -----------------------------------------------------------------------------
// ROLE SELECTOR
// -----------------------------------------------------------------------------
#define LC29H_ROLE_UAS_ROVER 1
#define LC29H_ROLE_BASE_SURVEY 2
#define LC29H_ROLE_BASE_STATIC 3
// ESP32BtRoamer defaults to rover profile flow.
#define LC29H_ROLE LC29H_ROLE_UAS_ROVER

// -----------------------------------------------------------------------------
// GENERIC OPTIONS (all roles)
// -----------------------------------------------------------------------------
#define LC29H_CFG_SAVE 1
// Boot: RESTOREPAR, rover mode, NMEA rates, SAVEPAR, PAIR023 (same order as the base).
#define LC29H_CFG_ESP32_BT_RUN_BRINGUP 1
// DA NMEA traffic can starve PQTM query replies during setup. Skip live
// verify so SAVEPAR + PAIR023 can apply; UART sniffer already proves RX.
#define LC29H_CFG_VERIFY 0
#define LC29H_CFG_ENABLE_RTCM 1

#define LC29H_CFG_BRIDGE_MODE_FORWARD_ALL 0
#define LC29H_CFG_BRIDGE_MODE_RTCM_ONLY 1
#define LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST 2
#define LC29H_CFG_BRIDGE_MODE LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST

#define LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED 1
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_GGA 1
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_GST 0
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_RMC 0
#define LC29H_CFG_BRIDGE_FORWARD_PQTM_STATUS 0

#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_NONE 0
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY 1
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_RAW_BINARY 2
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY

#define LC29H_CFG_RECOVERY_COMMAND_RETRIES 1
#define LC29H_CFG_RECOVERY_QUERY_RETRIES 1
#define LC29H_CFG_RECOVERY_RAW_WRITE_RETRIES 1
#define LC29H_CFG_RECOVERY_RETRY_DELAY_MS 10
#define LC29H_CFG_RECOVERY_EMIT_EVENTS 1

// -----------------------------------------------------------------------------
// ROVER OPTIONS
// -----------------------------------------------------------------------------
#define LC29H_CFG_FIX_RATE_MS 1000

#define LC29H_CFG_ROVER_PRINT_LOCAL_NMEA 0
#define LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK 1

// Phone GIS needs GGA (pos/alt/time/fix), RMC (time), GST (error).
// GSV/GSA/VTG stay off the air so UART and BLE can drain.
#define LC29H_CFG_BLE_NMEA_GGA_MS 1000
#define LC29H_CFG_BLE_NMEA_RMC_MS 1000
#define LC29H_CFG_BLE_NMEA_GST_MS 1000
#define LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE 256
#define LC29H_CFG_ROVER_ACCURACY_TRACK_ENABLE 0
#define LC29H_CFG_ROVER_ACCURACY_TRACK_WINDOW_MIN 60
#define LC29H_CFG_ROVER_ACCURACY_TRACK_MAX_POINTS 0

// Bluetooth rover link settings.
// BT_NAME is what your phone app will connect to.
// BT_PIN can be left empty for no pairing pin requirement.
#define LC29H_CFG_ESP32_BT_NAME "LC29H-Rover"
#define LC29H_CFG_ESP32_BT_PIN ""

// GNSS UART routing for this rover board.
// ESP32-S3 N16R8: RX=17, TX=18 (avoid GPIO16 conflicts with onboard peripherals).
#define LC29H_CFG_ESP32_BT_GNSS_RX_PIN 17
#define LC29H_CFG_ESP32_BT_GNSS_TX_PIN 18
#define LC29H_CFG_ESP32_BT_GNSS_BAUD 115200
#define LC29H_CFG_ESP32_RX_BUFFER_SIZE 8192

// Sketch-local GNSS UART sniffer. 1 = echo RX lines (and library TX debug)
// to USB Serial. On ESP32-S3 also copies to UART0 (CH343). 0 = off.
#ifndef LC29H_CFG_DEBUG_MIRROR_GNSS_UART
#define LC29H_CFG_DEBUG_MIRROR_GNSS_UART 0
#endif
// 1 = 1 Hz status + events on ESP32-S3 UART0 (CH343 USB-C). Not native USB.
#ifndef LC29H_CFG_DEBUG_UART0
#define LC29H_CFG_DEBUG_UART0 1
#endif

// Sketch-local WS2812 status LED. ESP32-S3-DevKitC-1 RGB is GPIO 48.
// Some S3 v1.1 boards use 38. Set to -1 to disable.
#define LC29H_CFG_STATUS_RGB_PIN 48

// -----------------------------------------------------------------------------
// BASE STATION OPTIONS
// -----------------------------------------------------------------------------
#define LC29H_CFG_SURVEY_MIN_TIME_SEC 3600
#define LC29H_CFG_SURVEY_MIN_STDDEV_M 1.5f
#define LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE 0
#define LC29H_CFG_BASE_ACCURACY_TRACK_WINDOW_SEC LC29H_CFG_SURVEY_MIN_TIME_SEC
#define LC29H_CFG_BASE_ACCURACY_TRACK_MAX_POINTS 0
#define LC29H_CFG_FINALIZE_SURVEY_TO_FIXED 1
#define LC29H_CFG_SURVEY_CAPTURE_TIMEOUT_MS 2000

#define LC29H_CFG_BASE_LAT_DEG 33.259933
#define LC29H_CFG_BASE_LON_DEG -97.897003
#define LC29H_CFG_BASE_ALT_M 276.0
