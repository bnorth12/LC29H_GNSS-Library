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
    Serial.println("SimpleBaseStation example");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required. Copy lc29hconfig.h.template into your sketch folder and rename it.");
        return;
    }

    LC29H_GNSS::ProfileResult result{
        LC29H_GNSS::ProfileStatus::CommandFailed,
        false,
    };
    LC29H_applyProjectConfig(gnss, result);
    Serial.print("Project config status=");
    Serial.println(profileStatusName(result.status));

    if (result.status != LC29H_GNSS::ProfileStatus::Success) {
        return;
    }

    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.querySurveyIn();
    Serial.println("Base survey profile ready. Type help for commands.");
}

void loop() {
    gnss.processSerialCommands();

    String line;
    if (gnss.readLine(line, 0)) {
        Serial.println(line);
    }
}