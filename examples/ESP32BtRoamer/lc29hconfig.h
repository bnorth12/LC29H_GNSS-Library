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
#define LC29H_CFG_VERIFY 1
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
#define LC29H_CFG_FIX_RATE_MS 200

#define LC29H_CFG_ROVER_PRINT_LOCAL_NMEA 1
// Set to 1 to send NMEA lines back to the connected Bluetooth app:
// - SPP text lines on classic ESP32
// - BLE notify text chunks on ESP32-S3
#define LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK 1
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
#define LC29H_CFG_ESP32_BT_GNSS_RX_PIN 16
#define LC29H_CFG_ESP32_BT_GNSS_TX_PIN 17
#define LC29H_CFG_ESP32_BT_GNSS_BAUD 115200

// -----------------------------------------------------------------------------
// BASE STATION OPTIONS
// -----------------------------------------------------------------------------
// Unused unless ROLE is switched to BASE_SURVEY. AccLimit 15 m starts Obs on DA.
#define LC29H_CFG_SURVEY_MIN_TIME_SEC 3600
#define LC29H_CFG_SURVEY_MIN_STDDEV_M 15.0f
#define LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE 0
#define LC29H_CFG_BASE_ACCURACY_TRACK_WINDOW_SEC LC29H_CFG_SURVEY_MIN_TIME_SEC
#define LC29H_CFG_BASE_ACCURACY_TRACK_MAX_POINTS 0
#define LC29H_CFG_FINALIZE_SURVEY_TO_FIXED 0
#define LC29H_CFG_SURVEY_CAPTURE_TIMEOUT_MS 2000

#define LC29H_CFG_BASE_LAT_DEG 33.259933
#define LC29H_CFG_BASE_LON_DEG -97.897003
#define LC29H_CFG_BASE_ALT_M 276.0
