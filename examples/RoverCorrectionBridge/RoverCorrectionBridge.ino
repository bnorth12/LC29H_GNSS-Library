#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

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

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_PRINT_LOCAL_NMEA)
constexpr bool kPrintLocalNmea = (LC29H_CFG_ROVER_PRINT_LOCAL_NMEA != 0);
#else
constexpr bool kPrintLocalNmea = true;
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK)
constexpr bool kForwardNmeaToLink = (LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK != 0);
#else
constexpr bool kForwardNmeaToLink = false;
#endif

#if LC29H_PROJECT_CONFIG_AVAILABLE && defined(LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE)
constexpr size_t kCorrectionChunkSize = LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE;
#else
constexpr size_t kCorrectionChunkSize = 256;
#endif

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = 16;
constexpr int kGnssTxPin = 17;
constexpr int kLinkRxPin = 18;
constexpr int kLinkTxPin = 19;
HardwareSerial& gnssPort = Serial1;
HardwareSerial& correctionLinkPort = Serial2;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
constexpr int kLinkRxPin = 6;
constexpr int kLinkTxPin = 5;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
SoftwareSerial correctionLinkPort(kLinkRxPin, kLinkTxPin);
#endif

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::RawIngressStats ingressStats;
LC29H_GNSS::BridgeState bridgeState;
LC29H_GNSS::BridgeStats bridgeStats;
LC29H_GNSS::BridgeMode bridgeMode = LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist;
LC29H_GNSS::BridgeNmeaFilter bridgeFilter;
bool bridgeFilterEnabled = true;
bool roverEnabled = false;
uint32_t lastStatusMs = 0;
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

#if defined(ARDUINO_ARCH_ESP32)
    gnssPort.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
    correctionLinkPort.begin(kLinkBaud, SERIAL_8N1, kLinkRxPin, kLinkTxPin);
#else
    gnssPort.begin(kGnssBaud);
    correctionLinkPort.begin(kLinkBaud);
    correctionLinkPort.listen();
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("RoverCorrectionBridge example");
    Serial.println("Ingest RTCM from the link UART and write it raw to GNSS. Drain GNSS every loop.");
    Serial.print("Local NMEA print=");
    Serial.println(kPrintLocalNmea ? "on" : "off");
    Serial.print("Forward NMEA to link=");
    Serial.println(kForwardNmeaToLink ? "on" : "off");
    Serial.println("Bridge allowlist defaults: GGA on, GST off, RMC off, PQTM off.");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for RoverCorrectionBridge. Example stays disabled.");
        return;
    }

    LC29H_GNSS::ProfileResult profileResult{
        LC29H_GNSS::ProfileStatus::CommandFailed,
        false,
    };
    roverEnabled = LC29H_applyProjectConfig(gnss, profileResult) &&
        profileResult.status == LC29H_GNSS::ProfileStatus::Success;

    if (roverEnabled && profileResult.powerCycleRecommended) {
        Serial.println("Rebooting module (PAIR023). PAIR003/PAIR002 sleep is not enough.");
        gnss.rebootModule();
        delay(3000);
    }

    bridgeMode = LC29H_projectBridgeMode();
    bridgeFilter = LC29H_projectBridgeNmeaFilter();
    bridgeFilterEnabled = LC29H_projectBridgeNmeaFilterEnabled();

    gnss.queryVersion();
    gnss.queryReceiverMode();
    Serial.println(roverEnabled ? "Rover correction ingress active." : "Rover startup failed.");
}

void loop() {
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif

    if (!roverEnabled) {
        return;
    }

#if !defined(ARDUINO_ARCH_ESP32)
    correctionLinkPort.listen();
#endif
    gnss.ingestRawAvailable(correctionLinkPort, 0, &ingressStats, kCorrectionChunkSize);

#if !defined(ARDUINO_ARCH_ESP32)
    gnssPort.listen();
#endif

    if (kForwardNmeaToLink) {
        Stream* localNmeaOut = kPrintLocalNmea ? &Serial : nullptr;
        gnss.forwardBridgeAvailable(
            correctionLinkPort,
            bridgeState,
            bridgeMode,
            bridgeFilter,
            bridgeFilterEnabled,
            &bridgeStats,
            0,
            localNmeaOut,
            nullptr);
    } else if (kPrintLocalNmea) {
        String line;
        while (gnss.readLine(line, 0)) {
            Serial.println(line);
        }
    }

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printRawIngressStatus(Serial, "RoverCorrectionBridge", ingressStats, now);
        if (kForwardNmeaToLink) {
            LC29H_GNSS::printBridgeStatus(Serial, "RoverNmeaForward", bridgeMode, bridgeStats, now);
        }
        lastStatusMs = now;
    }
}