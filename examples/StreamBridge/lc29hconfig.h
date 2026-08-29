#pragma once

// Example-local project config for StreamBridge.

// -----------------------------------------------------------------------------
// ROLE SELECTOR
// -----------------------------------------------------------------------------
#define LC29H_ROLE_UAS_ROVER 1
#define LC29H_ROLE_BASE_SURVEY 2
#define LC29H_ROLE_BASE_STATIC 3
// StreamBridge defaults to survey base profile flow.
#define LC29H_ROLE LC29H_ROLE_BASE_SURVEY

// -----------------------------------------------------------------------------
// GENERIC OPTIONS (all roles)
// -----------------------------------------------------------------------------
// SAVE=1 writes config to module flash.
// VERIFY=1 runs readback checks after profile apply.
#define LC29H_CFG_SAVE 1
#define LC29H_CFG_VERIFY 1
// Used by base-oriented workflows and bridge examples.
#define LC29H_CFG_ENABLE_RTCM 1

// Bridge mode selections.
#define LC29H_CFG_BRIDGE_MODE_FORWARD_ALL 0
#define LC29H_CFG_BRIDGE_MODE_RTCM_ONLY 1
#define LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST 2
// Recommended for base-to-rover/caster transport.
#define LC29H_CFG_BRIDGE_MODE LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST

// NMEA/PQTM allowlist when BRIDGE_MODE=RTCM_AND_NMEA_ALLOWLIST.
// Set BRIDGE_NMEA_FILTER_ENABLED=0 to forward all NMEA lines.
#define LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED 1
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_GGA 1
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_GST 0
#define LC29H_CFG_BRIDGE_FORWARD_NMEA_RMC 0
#define LC29H_CFG_BRIDGE_FORWARD_PQTM_STATUS 0

// Local debug output mode for bridge helpers.
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_NONE 0
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY 1
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_RAW_BINARY 2
#define LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE LC29H_CFG_LOCAL_DEBUG_OUTPUT_NMEA_ONLY

// Retry policy for command/query/raw transport failures.
#define LC29H_CFG_RECOVERY_COMMAND_RETRIES 1
#define LC29H_CFG_RECOVERY_QUERY_RETRIES 1
#define LC29H_CFG_RECOVERY_RAW_WRITE_RETRIES 1
#define LC29H_CFG_RECOVERY_RETRY_DELAY_MS 10
#define LC29H_CFG_RECOVERY_EMIT_EVENTS 1

// -----------------------------------------------------------------------------
// ROVER OPTIONS
// -----------------------------------------------------------------------------
// Rover profile fix interval (ms).
#define LC29H_CFG_FIX_RATE_MS 200

// Rover bridge behavior.
#define LC29H_CFG_ROVER_PRINT_LOCAL_NMEA 1
#define LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK 0
#define LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE 256
// Rover accuracy tracker (used by rover_status command).
#define LC29H_CFG_ROVER_ACCURACY_TRACK_ENABLE 0
#define LC29H_CFG_ROVER_ACCURACY_TRACK_WINDOW_MIN 60
// 0 = board-family default point count.
#define LC29H_CFG_ROVER_ACCURACY_TRACK_MAX_POINTS 0

// -----------------------------------------------------------------------------
// BASE STATION OPTIONS
// -----------------------------------------------------------------------------
// Survey-in: MinDur is fix-count at 1 Hz. AccLimit is meters (15 starts Obs on DA).
#define LC29H_CFG_SURVEY_MIN_TIME_SEC 3600
#define LC29H_CFG_SURVEY_MIN_STDDEV_M 15.0f
// Base accuracy tracker (used by survey_status command).
#define LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE 0
#define LC29H_CFG_BASE_ACCURACY_TRACK_WINDOW_SEC LC29H_CFG_SURVEY_MIN_TIME_SEC
// 0 = board-family default point count.
#define LC29H_CFG_BASE_ACCURACY_TRACK_MAX_POINTS 0
// Leave 0 while SVIN is running. Set 1 only after Valid=2 to write ECEF as fixed base.
#define LC29H_CFG_FINALIZE_SURVEY_TO_FIXED 0
#define LC29H_CFG_SURVEY_CAPTURE_TIMEOUT_MS 2000

// Fixed base coordinates for BASE_STATIC role.
#define LC29H_CFG_BASE_LAT_DEG 33.259933
#define LC29H_CFG_BASE_LON_DEG -97.897003
#define LC29H_CFG_BASE_ALT_M 276.0