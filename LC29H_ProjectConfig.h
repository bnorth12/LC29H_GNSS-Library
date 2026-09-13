#pragma once

#include <LC29H_GNSS.h>
#include <LC29H_MessageSchedule.h>
#include <LC29H_ModuleSetup.h>

// Project-level configuration loader.
//
// Expected user workflow:
// 1) Copy lc29hconfig.h.template into sketch folder as lc29hconfig.h
// 2) Set role and values
// 3) Call LC29H_bringUp(...) in setup() (adopt live SVIN, message rates, PAIR023)
//    or LC29H_applyProjectConfig(...) if you want the profile only.
//
// __has_include keeps this optional. If the file is absent, callers can
// choose whether to stop or fall back to manual/demo setup paths.
#if defined(__has_include)
#if __has_include("lc29hconfig.h")
#define LC29H_PROJECT_CONFIG_AVAILABLE 1
#include "lc29hconfig.h"
#else
#define LC29H_PROJECT_CONFIG_AVAILABLE 0
#endif
#else
#define LC29H_PROJECT_CONFIG_AVAILABLE 0
#endif

inline bool LC29H_projectConfigAvailable() {
#if LC29H_PROJECT_CONFIG_AVAILABLE
    return true;
#else
    return false;
#endif
}

// Bridge mode macros for project config.
// These let the examples stay fixed while per-project settings control whether
// the bridge forwards everything, RTCM-only, or RTCM plus selected NMEA lines.
#ifndef LC29H_CFG_BRIDGE_MODE_FORWARD_ALL
#define LC29H_CFG_BRIDGE_MODE_FORWARD_ALL 0
#endif

#ifndef LC29H_CFG_BRIDGE_MODE_RTCM_ONLY
#define LC29H_CFG_BRIDGE_MODE_RTCM_ONLY 1
#endif

#ifndef LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST
#define LC29H_CFG_BRIDGE_MODE_RTCM_AND_NMEA_ALLOWLIST 2
#endif

inline LC29H_GNSS::BridgeMode LC29H_projectBridgeMode() {
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_MODE)
#if (LC29H_CFG_BRIDGE_MODE == LC29H_CFG_BRIDGE_MODE_FORWARD_ALL)
    return LC29H_GNSS::BridgeMode::ForwardAll;
#elif (LC29H_CFG_BRIDGE_MODE == LC29H_CFG_BRIDGE_MODE_RTCM_ONLY)
    return LC29H_GNSS::BridgeMode::RtcmOnly;
#else
    return LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist;
#endif
#else
    return LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist;
#endif
}

inline LC29H_GNSS::BridgeNmeaFilter LC29H_projectBridgeNmeaFilter() {
    LC29H_GNSS::BridgeNmeaFilter f;

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_FORWARD_NMEA_GGA)
    f.forwardGga = (LC29H_CFG_BRIDGE_FORWARD_NMEA_GGA != 0);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_FORWARD_NMEA_GST)
    f.forwardGst = (LC29H_CFG_BRIDGE_FORWARD_NMEA_GST != 0);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_FORWARD_NMEA_RMC)
    f.forwardRmc = (LC29H_CFG_BRIDGE_FORWARD_NMEA_RMC != 0);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_FORWARD_PQTM_STATUS)
    f.forwardPqtm = (LC29H_CFG_BRIDGE_FORWARD_PQTM_STATUS != 0);
#endif

    return f;
}

inline bool LC29H_projectBridgeNmeaFilterEnabled() {
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED)
    return (LC29H_CFG_BRIDGE_NMEA_FILTER_ENABLED != 0);
#else
    // Default to filtering on so bridge examples stay conservative unless the
    // project explicitly disables the allowlist.
    return true;
#endif
}

inline LC29H_GNSS::LocalDebugOutputMode LC29H_projectLocalDebugOutputMode() {
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE)
#if (LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE == LC29H_CFG_LOCAL_DEBUG_OUTPUT_NONE)
    return LC29H_GNSS::LocalDebugOutputMode::None;
#elif (LC29H_CFG_LOCAL_DEBUG_OUTPUT_MODE == LC29H_CFG_LOCAL_DEBUG_OUTPUT_RAW_BINARY)
    return LC29H_GNSS::LocalDebugOutputMode::RawBinary;
#else
    return LC29H_GNSS::LocalDebugOutputMode::NmeaOnly;
#endif
#else
    // Default to readable local output so bench logs are useful without
    // flooding the console with binary data.
    return LC29H_GNSS::LocalDebugOutputMode::NmeaOnly;
#endif
}

inline LC29H_GNSS::RecoveryPolicy LC29H_projectRecoveryPolicy() {
    LC29H_GNSS::RecoveryPolicy p;

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_RECOVERY_COMMAND_RETRIES)
    p.commandRetries = static_cast<uint8_t>(LC29H_CFG_RECOVERY_COMMAND_RETRIES);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_RECOVERY_QUERY_RETRIES)
    p.queryRetries = static_cast<uint8_t>(LC29H_CFG_RECOVERY_QUERY_RETRIES);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_RECOVERY_RAW_WRITE_RETRIES)
    p.rawWriteRetries = static_cast<uint8_t>(LC29H_CFG_RECOVERY_RAW_WRITE_RETRIES);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_RECOVERY_RETRY_DELAY_MS)
    p.retryDelayMs = static_cast<uint16_t>(LC29H_CFG_RECOVERY_RETRY_DELAY_MS);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_RECOVERY_EMIT_EVENTS)
    p.emitRecoveryEvents = (LC29H_CFG_RECOVERY_EMIT_EVENTS != 0);
#endif

    return p;
}

inline LC29H_GNSS::AccuracyTrackerConfig LC29H_projectSurveyAccuracyTrackerConfig() {
    LC29H_GNSS::AccuracyTrackerConfig c;
    c.enabled = false;

#if defined(ARDUINO_ARCH_AVR)
    c.maxPoints = 48;
#elif defined(ARDUINO_ARCH_ESP32)
    c.maxPoints = 200;
#else
    c.maxPoints = 120;
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_SURVEY_MIN_TIME_SEC)
    c.windowSec = static_cast<uint32_t>(LC29H_CFG_SURVEY_MIN_TIME_SEC);
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE)
    c.enabled = (LC29H_CFG_BASE_ACCURACY_TRACK_ENABLE != 0);
#endif
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BASE_ACCURACY_TRACK_WINDOW_SEC)
    const uint32_t windowOverride = static_cast<uint32_t>(LC29H_CFG_BASE_ACCURACY_TRACK_WINDOW_SEC);
    if (windowOverride > 0) {
        c.windowSec = windowOverride;
    }
#endif
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_BASE_ACCURACY_TRACK_MAX_POINTS)
    const uint16_t pointsOverride = static_cast<uint16_t>(LC29H_CFG_BASE_ACCURACY_TRACK_MAX_POINTS);
    if (pointsOverride > 0) {
        c.maxPoints = pointsOverride;
    }
#endif
    return c;
}

inline LC29H_GNSS::AccuracyTrackerConfig LC29H_projectRoverAccuracyTrackerConfig() {
    LC29H_GNSS::AccuracyTrackerConfig c;
    c.enabled = false;
    c.windowSec = 3600;

#if defined(ARDUINO_ARCH_AVR)
    c.maxPoints = 48;
#elif defined(ARDUINO_ARCH_ESP32)
    c.maxPoints = 120;
#else
    c.maxPoints = 96;
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_ACCURACY_TRACK_ENABLE)
    c.enabled = (LC29H_CFG_ROVER_ACCURACY_TRACK_ENABLE != 0);
#endif
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_ACCURACY_TRACK_WINDOW_MIN)
    const uint32_t windowMinOverride = static_cast<uint32_t>(LC29H_CFG_ROVER_ACCURACY_TRACK_WINDOW_MIN);
    if (windowMinOverride > 0) {
        c.windowSec = windowMinOverride * 60UL;
    }
#endif
#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_ACCURACY_TRACK_MAX_POINTS)
    const uint16_t pointsOverride = static_cast<uint16_t>(LC29H_CFG_ROVER_ACCURACY_TRACK_MAX_POINTS);
    if (pointsOverride > 0) {
        c.maxPoints = pointsOverride;
    }
#endif
    return c;
}

inline bool LC29H_applyProjectConfig(LC29H_GNSS& gnss, LC29H_GNSS::ProfileResult& outResult) {
#if LC29H_PROJECT_CONFIG_AVAILABLE
    // Role dispatch intentionally uses compile-time macros so no runtime role parser
    // is required on constrained targets.
#if (LC29H_ROLE == LC29H_ROLE_UAS_ROVER)
    gnss.setSurveyAccuracyTrackerConfig(LC29H_GNSS::AccuracyTrackerConfig{});
    gnss.setRoverAccuracyTrackerConfig(LC29H_projectRoverAccuracyTrackerConfig());
    outResult = gnss.applyUasRoverProfile(LC29H_CFG_FIX_RATE_MS, LC29H_CFG_SAVE, LC29H_CFG_VERIFY);
    return true;
#elif (LC29H_ROLE == LC29H_ROLE_BASE_SURVEY)
    gnss.setSurveyAccuracyTrackerConfig(LC29H_projectSurveyAccuracyTrackerConfig());
    gnss.setRoverAccuracyTrackerConfig(LC29H_GNSS::AccuracyTrackerConfig{});
    outResult = gnss.applySurveyBaseProfile(
        LC29H_CFG_SURVEY_MIN_TIME_SEC,
        LC29H_CFG_SURVEY_MIN_STDDEV_M,
        LC29H_CFG_ENABLE_RTCM,
        LC29H_CFG_SAVE,
        LC29H_CFG_VERIFY);

#if LC29H_CFG_FINALIZE_SURVEY_TO_FIXED
    // Optional post-survey lock-in path. Leave this 0 while survey-in is running
    // (Valid=1): capturing ECEF at startup wipes or skips a live SVIN. Enable only
    // after Valid=2, then query surveyed ECEF -> write fixed ECEF -> optional save.
    if (outResult.status == LC29H_GNSS::ProfileStatus::Success) {
        LC29H_GNSS::PresetResult finalizeResult =
            gnss.finalizeSurveyInToFixedBase(LC29H_CFG_SURVEY_CAPTURE_TIMEOUT_MS, LC29H_CFG_SAVE);
        if (finalizeResult == LC29H_GNSS::PresetResult::CommandFailed) {
            outResult.status = LC29H_GNSS::ProfileStatus::CommandFailed;
        } else if (finalizeResult == LC29H_GNSS::PresetResult::SaveFailed) {
            outResult.status = LC29H_GNSS::ProfileStatus::SaveFailed;
        }
        outResult.powerCycleRecommended = gnss.isPowerCycleRecommended();
    }
#endif
    return true;
#elif (LC29H_ROLE == LC29H_ROLE_BASE_STATIC)
    gnss.setSurveyAccuracyTrackerConfig(LC29H_projectSurveyAccuracyTrackerConfig());
    gnss.setRoverAccuracyTrackerConfig(LC29H_GNSS::AccuracyTrackerConfig{});
    outResult = gnss.applyStaticBaseProfile(
        LC29H_CFG_BASE_LAT_DEG,
        LC29H_CFG_BASE_LON_DEG,
        LC29H_CFG_BASE_ALT_M,
        LC29H_CFG_ENABLE_RTCM,
        LC29H_CFG_SAVE,
        LC29H_CFG_VERIFY);
    return true;
#else
#error "Unsupported LC29H_ROLE in lc29hconfig.h"
#endif
#else
    outResult = {LC29H_GNSS::ProfileStatus::CommandFailed, false};
    (void)gnss;
    return false;
#endif
}

#ifndef LC29H_CFG_REBOOT_SETTLE_MS
#define LC29H_CFG_REBOOT_SETTLE_MS 3000
#endif

#ifndef LC29H_CFG_ESP32_RX_BUFFER_SIZE
#define LC29H_CFG_ESP32_RX_BUFFER_SIZE 8192
#endif

#ifndef LC29H_CFG_BOOT_CMD_PAUSE_MS
#define LC29H_CFG_BOOT_CMD_PAUSE_MS 150
#endif

#ifndef LC29H_CFG_FIX_RATE_MS
#define LC29H_CFG_FIX_RATE_MS 1000
#endif

struct LC29H_BringUpResult {
    LC29H_GNSS::ProfileResult profile{LC29H_GNSS::ProfileStatus::CommandFailed, false};
    bool adoptedLiveSurveyIn = false;
    LC29H_ModuleIdentity identity{};
};

#if defined(ARDUINO_ARCH_ESP32)
inline void LC29H_beginEsp32GnssUart(HardwareSerial& port, uint32_t baud, int rxPin, int txPin) {
    // setRxBufferSize must run before begin() or the driver keeps 256 bytes.
    port.setRxBufferSize(LC29H_CFG_ESP32_RX_BUFFER_SIZE);
    port.begin(baud, SERIAL_8N1, rxPin, txPin);
}
#endif

inline bool LC29H_accLimitMatches(float actualM, float desiredM) {
    const float delta = actualM - desiredM;
    return delta > -0.15f && delta < 0.15f;
}

inline void LC29H_rebootIfNeeded(LC29H_GNSS& gnss, bool recommended, Stream* log) {
    if (!recommended) {
        return;
    }
    if (log != nullptr) {
        log->println("Rebooting module (PAIR023). PAIR003/PAIR002 sleep is not enough.");
    }
    gnss.rebootModule();
    delay(LC29H_CFG_REBOOT_SETTLE_MS);
}

#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_UartPump.h>

// Drain UART during boot delays so NMEA/OK cannot fill the driver FIFO.
inline void LC29H_bootPause(
    uint32_t ms,
    HardwareSerial& gnssPort,
    LC29H_UartPump::Pump& pump,
    void (*tick)()) {
    const uint32_t start = millis();
    while ((millis() - start) < ms) {
        if (tick != nullptr) {
            tick();
        }
        pump.drain(gnssPort);
        pump.frame();
        delay(10);
    }
}

// Phone/BLE rover boot. Restore first so a swapped module is not left in base
// or Fitness nav. Identify PQTMVERNO before SAVEPAR. Call before advertising.
inline bool LC29H_roverFactoryBringUp(
    LC29H_GNSS& gnss,
    Stream* log,
    HardwareSerial& gnssPort,
    LC29H_UartPump::Pump& pump,
    void (*tick)() = nullptr,
    LC29H_ModuleIdentity* identityOut = nullptr) {
    auto pause = [&](uint32_t ms) {
        LC29H_bootPause(ms, gnssPort, pump, tick);
    };
    auto step = [&](const char* name) {
        if (log != nullptr) {
            log->print("CFG: ");
            log->println(name);
        }
    };

    step("identify PQTMVERNO");
    LC29H_ModuleIdentity id;
    gnss.queryVersion();
    {
        const uint32_t start = millis();
        while ((millis() - start) < 800) {
            if (tick != nullptr) {
                tick();
            }
            pump.drain(gnssPort);
            pump.frame();
            pump.processNmea(8, [](const char* line, void* user) {
                LC29H_noteIdentityLine(*static_cast<LC29H_ModuleIdentity*>(user), line);
            }, &id);
            if (id.family != LC29H_NmeaCompat::ModuleFamily::Unknown) {
                break;
            }
            delay(10);
        }
    }
    LC29H_printIdentity(log, id);
    if (identityOut != nullptr) {
        *identityOut = id;
    }
    if (id.family == LC29H_NmeaCompat::ModuleFamily::BS) {
        step("LC29H(BS) is a base-only module; rover bring-up aborted");
        return false;
    }
    LC29H_applyFamilyPolicy(gnss, id.family, true);
    pause(200);

    step("PQTMRESTOREPAR");
    gnss.restoreDefaults();
    pause(500);

    step("PQTMCFGRCVRMODE rover");
    gnss.setReceiverModeRover();
    pause(400);

    step("PAIR081 normal nav");
    gnss.sendPayload("PAIR081,0");
    pause(300);

    step("fix rate");
    gnss.setFixRateMs(LC29H_CFG_FIX_RATE_MS);
    pause(300);

    step("phone NMEA rates");
    LC29H_MessageSchedule::applyRoverPhoneRates(gnss);
    pause(400);

    step("PQTMSAVEPAR");
    gnss.saveConfig();
    pause(500);

    if (LC29H_familyUsesPair023(id.family)) {
        step("PAIR023 reboot");
        gnss.rebootModule();
        pause(LC29H_CFG_REBOOT_SETTLE_MS);
    } else {
        step("skip PAIR023 (this variant)");
    }
    step("rover config done");
    return true;
}
#endif

// Role bring-up used by the examples:
// - Base survey: adopt a matching in-progress SVIN (no CFGSVIN/PAIR023).
//   Otherwise apply the survey profile, status NMEA schedule, SAVEPAR, PAIR023.
// - Rover: rover profile, GIS NMEA (GGA/RMC every epoch), SAVEPAR, PAIR023 if needed.
// - Static base: static profile, base status NMEA schedule, SAVEPAR, PAIR023 if needed.
inline bool LC29H_bringUp(LC29H_GNSS& gnss, LC29H_BringUpResult& out, Stream* log = nullptr) {
    out = LC29H_BringUpResult{};
    out.identity = LC29H_identifyModule(gnss, log);

#if !LC29H_PROJECT_CONFIG_AVAILABLE
    (void)gnss;
    (void)log;
    return false;
#elif (LC29H_ROLE == LC29H_ROLE_BASE_SURVEY)
    gnss.setSurveyAccuracyTrackerConfig(LC29H_projectSurveyAccuracyTrackerConfig());
    gnss.setRoverAccuracyTrackerConfig(LC29H_GNSS::AccuracyTrackerConfig{});

    LC29H_GNSS::SurveyInConfig cfg;
    if (gnss.getSurveyInConfig(cfg) &&
        cfg.mode == 1 &&
        cfg.minDur == static_cast<uint32_t>(LC29H_CFG_SURVEY_MIN_TIME_SEC) &&
        LC29H_accLimitMatches(cfg.accLimitM, LC29H_CFG_SURVEY_MIN_STDDEV_M)) {
        out.adoptedLiveSurveyIn = true;
        out.profile.status = LC29H_GNSS::ProfileStatus::Success;
        out.profile.powerCycleRecommended = false;
        if (log != nullptr) {
            log->print("Adopting live survey-in MinDur=");
            log->print(cfg.minDur);
            log->println(" (skipping CFGSVIN/PAIR023 so Obs is not reset).");
        }
        LC29H_MessageSchedule::applyBaseStatusRates(gnss);
        gnss.saveConfig();
        return true;
    }

    if (!LC29H_applyProjectConfig(gnss, out.profile) ||
        out.profile.status != LC29H_GNSS::ProfileStatus::Success) {
        return false;
    }
    LC29H_MessageSchedule::applyBaseStatusRates(gnss);
    gnss.saveConfig();
    if (LC29H_familyUsesPair023(out.identity.family)) {
        LC29H_rebootIfNeeded(gnss, out.profile.powerCycleRecommended, log);
    }
    return true;

#elif (LC29H_ROLE == LC29H_ROLE_UAS_ROVER)
    LC29H_applyFamilyPolicy(gnss, out.identity.family, true);
    if (!LC29H_applyProjectConfig(gnss, out.profile) ||
        out.profile.status != LC29H_GNSS::ProfileStatus::Success) {
        return false;
    }
    LC29H_MessageSchedule::applyRoverGisRates(gnss, LC29H_CFG_FIX_RATE_MS);
    gnss.saveConfig();
    if (LC29H_familyUsesPair023(out.identity.family)) {
        LC29H_rebootIfNeeded(gnss, out.profile.powerCycleRecommended, log);
    }
    return true;

#elif (LC29H_ROLE == LC29H_ROLE_BASE_STATIC)
    LC29H_applyFamilyPolicy(gnss, out.identity.family, false);
    if (!LC29H_applyProjectConfig(gnss, out.profile) ||
        out.profile.status != LC29H_GNSS::ProfileStatus::Success) {
        return false;
    }
    LC29H_MessageSchedule::applyBaseStatusRates(gnss);
    gnss.saveConfig();
    if (LC29H_familyUsesPair023(out.identity.family)) {
        LC29H_rebootIfNeeded(gnss, out.profile.powerCycleRecommended, log);
    }
    return true;

#else
#error "Unsupported LC29H_ROLE in lc29hconfig.h"
#endif
}
