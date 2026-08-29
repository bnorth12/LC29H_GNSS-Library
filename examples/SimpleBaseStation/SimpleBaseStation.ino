#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// Simple survey-base bring-up. Not a full NTRIP/Wi-Fi app.
//
// 1) AccLimit 15 m. Matching live SVIN is adopted (no PAIR023, Obs kept).
// 2) Otherwise CFGSVIN + SAVEPAR + PAIR023. PAIR003 sleep is not enough.
// 3) Status NMEA schedule: GGA/RMC 1 s, GST/GSA/EPE 5 s, GSV/SVIN 10 s.
//    RTCM MSM7+1005 stay 1 Hz (mission). NMEA is status.
// 4) Drain mixed NMEA+RTCM every loop. MeanAcc 0.0000 is not "complete".
// Message payloads: this folder's README and library Readme "Module messages in practice".
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

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = 16;
constexpr int kGnssTxPin = 17;
HardwareSerial& gnssPort = Serial1;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
#endif

class DiscardStream : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t) override { return 1; }
};

LC29H_GNSS gnss(gnssPort, &Serial);
DiscardStream discardRtcm;
LC29H_GNSS::BridgeState bridgeState;
LC29H_GNSS::BridgeStats bridgeStats;
LC29H_GNSS::BridgeNmeaFilter nmeaFilter;
bool exampleEnabled = false;

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
    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
#else
    gnssPort.begin(kGnssBaud);
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("SimpleBaseStation example");
    Serial.println("RTCM 1 Hz mission. Adopt live SVIN if MinDur matches.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required. Copy lc29hconfig.h.template into your sketch folder and rename it.");
        return;
    }

    LC29H_BringUpResult bringUp;
    if (!LC29H_bringUp(gnss, bringUp, &Serial)) {
        Serial.print("Bring-up failed, status=");
        Serial.println(profileStatusName(bringUp.profile.status));
        return;
    }

    exampleEnabled = true;
    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.querySurveyIn();
    Serial.println("Base survey ready. Watch $PQTMSVINSTATUS Valid=1 Obs counting.");
    Serial.println("MeanAcc 0.0000 is a placeholder, not complete. survey_finalize only after Valid=2.");
}

void loop() {
    gnss.processSerialCommands();
    if (!exampleEnabled) {
        return;
    }

    // Pump every loop. Print complete NMEA after the parser; do not parse here.
    gnss.forwardBridgeAvailable(
        discardRtcm,
        bridgeState,
        LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist,
        nmeaFilter,
        false,
        &bridgeStats,
        0,
        &Serial,
        nullptr);
}
