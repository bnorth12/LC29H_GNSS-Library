#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// Simple rover: ingest RTCM corrections, publish GGA (position) and RMC (time)
// every epoch for GIS mapping tools. GST/GSA/ZDA ~1 Hz, GSV every 10 s.
//
// ESP32: GNSS on Serial1, correction UART on Serial2. Ingest corrections first
// every loop, then drain NMEA. AVR: NMEA only; use RoverCorrectionBridge for RTCM in.
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
constexpr uint32_t kCorrBaud = 115200;
constexpr uint32_t kStatusIntervalMs = 1000;

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = 16;
constexpr int kGnssTxPin = 17;
constexpr int kCorrRxPin = 5;
constexpr int kCorrTxPin = 4;
HardwareSerial& gnssPort = Serial1;
HardwareSerial& corrPort = Serial2;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
#endif

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::RawIngressStats corrStats;
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
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

#if defined(ARDUINO_ARCH_ESP32)
    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
    corrPort.begin(kCorrBaud, SERIAL_8N1, kCorrRxPin, kCorrTxPin);
#else
    gnssPort.begin(kGnssBaud);
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("SimpleRover example");
    Serial.println("Mission: RTCM in, GGA+RMC out (GIS). GSV every 10 s.");

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
    gnss.queryFixRate();
#if defined(ARDUINO_ARCH_ESP32)
    Serial.print("Correction UART Serial2 RX=");
    Serial.print(kCorrRxPin);
    Serial.print(", TX=");
    Serial.println(kCorrTxPin);
#else
    Serial.println("AVR build: NMEA only. Use RoverCorrectionBridge to ingest RTCM.");
#endif
    Serial.println("Rover ready. Type help for commands.");
}

void loop() {
    gnss.processSerialCommands();
    if (!exampleEnabled) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    // Corrections first. Do not let NMEA printing starve RTCM ingress.
    gnss.ingestRawAvailable(corrPort, 0, &corrStats, 256);
#endif

    String line;
    while (gnss.readLine(line, 0)) {
        Serial.println(line);
    }

#if defined(ARDUINO_ARCH_ESP32)
    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printRawIngressStatus(Serial, "SimpleRover", corrStats, now);
        lastStatusMs = now;
    }
#endif
}
