#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// Simple survey-base bring-up. Not a full NTRIP/Wi-Fi app.
//
// 1) AccLimit 15 m (Quectel DA default). 0 / 1.5 / 2 left <Obs> at 0.
// 2) Profile writes mode + CFGSVIN + RTCM and SAVEPAR.
// 3) Lower bulky NMEA: GSV and PQTMSVINSTATUS RATE 10 (every 10 s at 1 Hz).
//    RTCM MSM7+1005 stay 1 Hz (mission stream). NMEA is status.
// 4) SAVEPAR then rebootModule() (PAIR023). PAIR003/PAIR002 sleep is not enough.
// 5) Drain the GNSS UART every loop. One reader. Do not query* while draining.
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
constexpr uint32_t kRebootSettleMs = 3000;

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

// Status NMEA is not the mission stream. RATE is every N 1 Hz epochs.
void applyStatusMessageRates() {
    gnss.setMessageRate("GSV", 1, 10);
    gnss.setMessageRate("PQTMSVINSTATUS", 1, 10);
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
    Serial.println("AccLimit 15 m, GSV/SVIN RATE 10, SAVEPAR + PAIR023.");

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

    applyStatusMessageRates();
    if (!gnss.saveConfig()) {
        Serial.println("Save after message rates failed.");
        return;
    }

    if (result.powerCycleRecommended) {
        Serial.println("Rebooting module (PAIR023). PAIR003/PAIR002 sleep is not enough.");
        gnss.rebootModule();
        delay(kRebootSettleMs);
    }

    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.querySurveyIn();
    Serial.println("Base survey profile ready. Type help for commands.");
    Serial.println("Watch $PQTMSVINSTATUS: Valid=1 and Obs counting. survey_finalize only after Valid=2.");
}

void loop() {
    // Typed console commands also read the GNSS UART. Use them between bursts,
    // not as a second concurrent reader while a production pump owns the port.
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif

    String line;
    if (gnss.readLine(line, 0)) {
        Serial.println(line);
    }
}
