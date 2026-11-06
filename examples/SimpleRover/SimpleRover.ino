#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <lc29h_board.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_UartPump.h>
#include <LC29H_HostPump.h>
#include <LC29H_NmeaFeed.h>
#endif

// Simple rover: RTCM in is the mission; GGA/RMC out for GIS.
//
// Boot: identify PQTMVERNO, rover mode, SAVEPAR, PAIR023 on DA/EA.
// Do NOT set GNSS UART read timeout to 0 until after bring-up — zero timeout
// under 10 Hz NMEA starves reply matching (issue #3 Scope B).
//
// SAVEPAR / module flash: LC29H_CFG_SAVE=1 writes rover rates/mode into the
// module. Restore the base-station message-rate set afterward (#12).
//
// ESP32-S3: USB CDC On Boot = CDC. Pins from lc29h_board.h (RX17/TX18).

#if !defined(ARDUINO_ARCH_ESP32)
#include <SoftwareSerial.h>
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = LC29H_BOARD_GNSS_BAUD;
constexpr uint32_t kCorrBaud = 115200;
constexpr uint32_t kStatusIntervalMs = 1000;

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = LC29H_BOARD_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_BOARD_GNSS_TX_PIN;
constexpr int kCorrRxPin = LC29H_BOARD_CORR_RX_PIN;
constexpr int kCorrTxPin = LC29H_BOARD_CORR_TX_PIN;
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
    // Keep default (blocking) timeout through bring-up / verify.
    uartPump.setPriorities(LC29H_UartPump::roverGisPriorities());
    corrPort.begin(kCorrBaud, SERIAL_8N1, kCorrRxPin, kCorrTxPin);
#else
    gnssPort.begin(kGnssBaud);
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("SimpleRover example");
    Serial.print("GNSS UART RX=");
    Serial.print(kGnssRxPin);
    Serial.print(" TX=");
    Serial.println(kGnssTxPin);
    Serial.println("Mission: RTCM in, GGA+RMC out (GIS).");
    Serial.println("NOTE: SAVEPAR may leave the module non-default; restore BS rates after HIL.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("HIL_EXAMPLE=SimpleRover STATUS=DISABLED reason=no_lc29hconfig");
        return;
    }

    LC29H_BringUpResult bringUp;
    if (!LC29H_bringUp(gnss, bringUp, &Serial)) {
        Serial.print("Bring-up failed, status=");
        Serial.println(profileStatusName(bringUp.profile.status));
        Serial.print("HIL_EXAMPLE=SimpleRover STATUS=FAIL bringup=");
        Serial.println(profileStatusName(bringUp.profile.status));
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    gnssPort.setTimeout(0);
#endif

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
    Serial.println("HIL_EXAMPLE=SimpleRover STATUS=READY");
}

void loop() {
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif
    if (!exampleEnabled) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    gnss.ingestRawAvailable(corrPort, 0, &corrStats, 256);
    LC29H_HostPump::tick(gnssPort, uartPump);
    LC29H_HostPump::processTo(uartPump, nullptr, &Serial);
    // Note: RTCM from GNSS is uncommon on rover; nullptr OK. Corr path is Serial2 ingest.

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printRawIngressStatus(Serial, "SimpleRover", corrStats, now);
        const LC29H_UartPump::PumpHealth health = LC29H_HostPump::snapshotHealth(uartPump);
        Serial.print("pump frames=");
        Serial.print(health.frames);
        Serial.print(" mailboxOverwrites=");
        Serial.print(health.drops);
        Serial.print(" usbCdcBackpressureDrops=");
        Serial.print(LC29H_HostPump::txDropCount());
        Serial.print(" high=");
        Serial.print(health.drainHighWater);
        Serial.print(" gap=");
        Serial.println(health.maxGapMs);
        lastStatusMs = now;
    }
#else
    String line;
    while (gnss.readLine(line, 0)) {
        Serial.println(line);
    }
#endif
}