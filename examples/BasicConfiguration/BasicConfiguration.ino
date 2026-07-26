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

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = 16;
constexpr int kGnssTxPin = 17;
HardwareSerial& gnssPort = Serial1;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
#endif

LC29H_GNSS gnss(gnssPort, &Serial);

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

void printProfileResult(const char* label, const LC29H_GNSS::ProfileResult& result) {
    Serial.print(label);
    Serial.print(": status=");
    Serial.print(profileStatusName(result.status));
    Serial.print(", powerCycleRecommended=");
    Serial.println(result.powerCycleRecommended ? "yes" : "no");
}

void printStartupPins() {
    Serial.print("GNSS UART pins RX=");
    Serial.print(kGnssRxPin);
    Serial.print(", TX=");
    Serial.println(kGnssTxPin);
}
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

#if defined(ARDUINO_ARCH_ESP32)
    gnssPort.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);
#else
    gnssPort.begin(kGnssBaud);
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("BasicConfiguration example");
    printStartupPins();

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required. Copy lc29hconfig.h.template into your sketch folder and rename it.");
        return;
    }

    LC29H_GNSS::ProfileResult profileResult{
        LC29H_GNSS::ProfileStatus::CommandFailed,
        false,
    };
    LC29H_applyProjectConfig(gnss, profileResult);
    printProfileResult("Project config", profileResult);

    if (profileResult.status != LC29H_GNSS::ProfileStatus::Success) {
        return;
    }

    gnss.queryVersion();
    gnss.queryQVersion();
    gnss.queryUniqueId();
    gnss.queryReceiverMode();
    gnss.queryFixRate();
    gnss.queryBaudRate();

    if (gnss.hasCapturedSurveyEcef()) {
        const LC29H_GNSS::EcefPosition ecef = gnss.getCapturedSurveyEcef();
        Serial.print("Captured ECEF X=");
        Serial.print(ecef.x, 3);
        Serial.print(", Y=");
        Serial.print(ecef.y, 3);
        Serial.print(", Z=");
        Serial.println(ecef.z, 3);
    }

    Serial.println("Type help in Serial Monitor for interactive commands.");
}

void loop() {
    gnss.processSerialCommands();

    String line;
    if (gnss.readLine(line, 0)) {
        LC29H_GNSS::PairAck ack;
        if (LC29H_GNSS::tryParsePairAck(line, ack)) {
            LC29H_GNSS::printPairAck(Serial, ack, "ACK");
        } else {
            Serial.println(line);
        }
    }
}