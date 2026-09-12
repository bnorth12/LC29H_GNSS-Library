#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_UartPump.h>
#endif

// Simple rover: RTCM in is the mission; GGA/RMC out for GIS.
//
// Why ESP32 uses UartPump instead of while(readLine): a live LC29H can emit
// GSV+GSA faster than Serial.println. Unbounded readLine starves the correction
// UART and overflows the GNSS RX FIFO. Drain/frame every loop; ingest RTCM first.
//
// Boot calls LC29H_bringUp() which identifies PQTMVERNO, then rover mode,
// SAVEPAR, PAIR023 on DA/EA. Swap modules later: Serial "module_reinit rover".
// AVR: no pump (SRAM). NMEA only; use RoverCorrectionBridge for a second UART.
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
#if defined(ARDUINO_ARCH_ESP32)
LC29H_UartPump::Pump uartPump;
#endif
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
    gnssPort.setTimeout(0);
    uartPump.setPriorities(LC29H_UartPump::roverGisPriorities());
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
    Serial.print("Detected module family=");
    Serial.println(LC29H_NmeaCompat::familyName(bringUp.identity.family));
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
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif
    if (!exampleEnabled) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    // Corrections first. Printing NMEA before this will drop MSM/1005.
    gnss.ingestRawAvailable(corrPort, 0, &corrStats, 256);
    uartPump.drain(gnssPort);  // empty the driver FIFO (overwrite-oldest ring)
    uartPump.frame();          // NMEA XOR + RTCM length; bad checksums counted
    uartPump.processNmea(8, [](const char* line, void*) {
        if (line != nullptr) {
            Serial.println(line);
        }
    }, nullptr);
#else
    String line;
    while (gnss.readLine(line, 0)) {
        Serial.println(line);
    }
#endif

#if defined(ARDUINO_ARCH_ESP32)
    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printRawIngressStatus(Serial, "SimpleRover", corrStats, now);
        lastStatusMs = now;
    }
#endif
}
