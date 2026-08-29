#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// Bench base pair: parse GNSS UART and forward RTCM to a rover on a second UART.
// LC29H_bringUp() adopts a matching live SVIN or starts survey-in. RTCM stays 1 Hz.
// Pump every loop; do not parse inside the pump callback.
//
// Minimum verified hardware:
// - Arduino Mega 2560 class (AVR Uno/Nano class boards run out of RAM)
// - ESP32 class boards are also supported via their dedicated paths

#if !defined(ARDUINO_ARCH_ESP32)
#include <SoftwareSerial.h>
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = 115200;
constexpr uint32_t kLinkBaud = 115200;
constexpr uint32_t kStatusIntervalMs = 1000;

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = 16;
constexpr int kGnssTxPin = 17;
constexpr int kLinkRxPin = 18;
constexpr int kLinkTxPin = 19;
HardwareSerial& gnssPort = Serial1;
HardwareSerial& roverLinkPort = Serial2;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
constexpr int kLinkRxPin = 6;
constexpr int kLinkTxPin = 5;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
SoftwareSerial roverLinkPort(kLinkRxPin, kLinkTxPin);
#endif

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::BridgeState bridgeState;
LC29H_GNSS::BridgeStats bridgeStats;
LC29H_GNSS::BridgeMode bridgeMode = LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist;
LC29H_GNSS::BridgeNmeaFilter bridgeFilter;
LC29H_GNSS::LocalDebugOutputMode debugMode = LC29H_GNSS::LocalDebugOutputMode::NmeaOnly;
bool bridgeFilterEnabled = true;
bool bridgeEnabled = false;
uint32_t lastStatusMs = 0;
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

#if defined(ARDUINO_ARCH_ESP32)
    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
    roverLinkPort.begin(kLinkBaud, SERIAL_8N1, kLinkRxPin, kLinkTxPin);
#else
    gnssPort.begin(kGnssBaud);
    roverLinkPort.begin(kLinkBaud);
    gnssPort.listen();
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("BaseSerialBridge example");
    Serial.println("For bench use: GNSS output is forwarded to the rover link UART.");
    Serial.println("RTCM 1 Hz mission. Adopt live SVIN if MinDur matches. Status NMEA scheduled.");
    Serial.println("Bridge allowlist defaults: GGA on, GST off, RMC off, PQTM off.");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for BaseSerialBridge. Example stays disabled.");
        return;
    }

    LC29H_BringUpResult bringUp;
    bridgeEnabled = LC29H_bringUp(gnss, bringUp, &Serial);

    bridgeMode = LC29H_projectBridgeMode();
    bridgeFilter = LC29H_projectBridgeNmeaFilter();
    bridgeFilterEnabled = LC29H_projectBridgeNmeaFilterEnabled();
    debugMode = LC29H_projectLocalDebugOutputMode();

    gnss.queryVersion();
    gnss.queryReceiverMode();

    Serial.print("Bridge mode=");
    Serial.println(LC29H_GNSS::bridgeModeName(bridgeMode));
    Serial.println(bridgeEnabled ? "Base bridge active." : "Base bridge startup failed.");
}

void loop() {
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif

    if (!bridgeEnabled) {
        return;
    }

    Stream* localNmeaOut = nullptr;
    Stream* localRawOut = nullptr;

    if (debugMode == LC29H_GNSS::LocalDebugOutputMode::NmeaOnly) {
        localNmeaOut = &Serial;
    } else if (debugMode == LC29H_GNSS::LocalDebugOutputMode::RawBinary) {
        localRawOut = &Serial;
    }

    // Pump every loop. Do not parse or write flash inside localNmeaOut.
    gnss.forwardBridgeAvailable(
        roverLinkPort,
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
        LC29H_GNSS::printBridgeStatus(Serial, "BaseSerialBridge", bridgeMode, bridgeStats, now);
        lastStatusMs = now;
    }
}