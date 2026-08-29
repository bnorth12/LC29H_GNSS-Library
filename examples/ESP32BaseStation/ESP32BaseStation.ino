#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// ESP32 survey-base bring-up with RTCM forwarded on Serial2. Not a Wi-Fi/NTRIP app.
//
// AccLimit 15 m, GSV/SVIN RATE 10, SAVEPAR + PAIR023 (not PAIR003 sleep).
// RTCM at 1 Hz is the mission stream. NMEA is status.
// Pump GNSS UART every loop; do not parse inside the pump callback.
//
// Minimum verified hardware:
// - ESP32-S3 target (compiled with esp32:esp32:esp32s3)
// - Designed for dual USB-C ESP32-S3 dev boards with external GNSS UART wiring

#if !defined(ARDUINO_ARCH_ESP32)
#error "ESP32BaseStation.ino is intended for ESP32 targets only."
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = 115200;
constexpr uint32_t kRtcmBaud = 115200;
constexpr uint32_t kStatusIntervalMs = 1000;
constexpr uint32_t kRebootSettleMs = 3000;

constexpr int kGnssRxPin = LC29H_CFG_ESP32_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_CFG_ESP32_GNSS_TX_PIN;
constexpr int kRtcmRxPin = LC29H_CFG_ESP32_RTCM_RX_PIN;
constexpr int kRtcmTxPin = LC29H_CFG_ESP32_RTCM_TX_PIN;

HardwareSerial& gnssPort = Serial1;
HardwareSerial& rtcmPort = Serial2;

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::BridgeState bridgeState;
LC29H_GNSS::BridgeStats bridgeStats;
LC29H_GNSS::BridgeMode bridgeMode = LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist;
LC29H_GNSS::BridgeNmeaFilter bridgeFilter;
LC29H_GNSS::LocalDebugOutputMode debugMode = LC29H_GNSS::LocalDebugOutputMode::NmeaOnly;
bool bridgeFilterEnabled = true;
bool exampleEnabled = false;
uint32_t lastStatusMs = 0;

const char* profileStatusName(LC29H_GNSS::ProfileStatus status) {
    switch (status) {
    case LC29H_GNSS::ProfileStatus::Success:
        return "Success";
    case LC29H_GNSS::ProfileStatus::CommandFailed:
        return "CommandFailed";
    case LC29H_GNSS::ProfileStatus::SaveFailed:
        return "SaveFailed";
    case LC29H_GNSS::ProfileStatus::VerifyFailed:
        return "VerifyFailed";
    default:
        return "Unknown";
    }
}

void applyStatusMessageRates() {
    gnss.setMessageRate("GSV", 1, 10);
    gnss.setMessageRate("PQTMSVINSTATUS", 1, 10);
}
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

    gnssPort.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
    rtcmPort.begin(kRtcmBaud, SERIAL_8N1, kRtcmRxPin, kRtcmTxPin);

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("ESP32BaseStation example");
    Serial.println("ESP32-only base station setup with optional RTCM forwarding on Serial2.");
    Serial.println("AccLimit 15 m, GSV/SVIN RATE 10, SAVEPAR + PAIR023. RTCM stays 1 Hz.");
    Serial.println("Console output uses native USB Serial; edit lc29hconfig.h for your board UART GPIO mapping.");
    Serial.println("Bridge allowlist defaults: GGA on, GST off, RMC off, PQTM off.");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");
    Serial.print("GNSS UART: Serial1 RX=");
    Serial.print(kGnssRxPin);
    Serial.print(", TX=");
    Serial.println(kGnssTxPin);
    Serial.print("RTCM UART: Serial2 RX=");
    Serial.print(kRtcmRxPin);
    Serial.print(", TX=");
    Serial.println(kRtcmTxPin);

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for ESP32BaseStation. Example stays disabled.");
        return;
    }

    LC29H_GNSS::ProfileResult profileResult{
        LC29H_GNSS::ProfileStatus::CommandFailed,
        false,
    };
    LC29H_applyProjectConfig(gnss, profileResult);

    Serial.print("Project config status=");
    Serial.println(profileStatusName(profileResult.status));
    if (profileResult.powerCycleRecommended) {
        Serial.println("Power cycle recommended after configuration.");
    }

    if (profileResult.status != LC29H_GNSS::ProfileStatus::Success) {
        return;
    }

    applyStatusMessageRates();
    gnss.saveConfig();
    if (profileResult.powerCycleRecommended) {
        Serial.println("Rebooting module (PAIR023). PAIR003/PAIR002 sleep is not enough.");
        gnss.rebootModule();
        delay(kRebootSettleMs);
    }

    bridgeMode = LC29H_projectBridgeMode();
    bridgeFilter = LC29H_projectBridgeNmeaFilter();
    bridgeFilterEnabled = LC29H_projectBridgeNmeaFilterEnabled();
    debugMode = LC29H_projectLocalDebugOutputMode();
    exampleEnabled = true;

    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.querySurveyIn();

    Serial.print("Bridge mode=");
    Serial.println(LC29H_GNSS::bridgeModeName(bridgeMode));
    Serial.print("Local debug output=");
    Serial.println(LC29H_GNSS::localDebugOutputModeName(debugMode));
    Serial.println("ESP32 base station active.");
}

void loop() {
    // Console commands steal UART bytes from the pump. Fine for bench typing;
    // a production sketch should not mix processSerialCommands with the pump.
    gnss.processSerialCommands();

    if (!exampleEnabled) {
        return;
    }

    Stream* localNmeaOut = nullptr;
    Stream* localRawOut = nullptr;

    if (debugMode == LC29H_GNSS::LocalDebugOutputMode::NmeaOnly) {
        localNmeaOut = &Serial;
    } else if (debugMode == LC29H_GNSS::LocalDebugOutputMode::RawBinary) {
        localRawOut = &Serial;
    }

    // Pump every loop. Printing NMEA here is OK for a bench example; do not parse
    // Strings, walk survey history, or write flash inside this callback.
    gnss.forwardBridgeAvailable(
        rtcmPort,
        bridgeState,
        bridgeMode,
        bridgeFilter,
        bridgeFilterEnabled,
        &bridgeStats,
        0,
        localNmeaOut,
        localRawOut);

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printBridgeStatus(Serial, "ESP32BaseStation", bridgeMode, bridgeStats, now);
        lastStatusMs = now;
    }
}