#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_HostPump.h>
#include <LC29H_Rtcm.h>
#endif

// Bench rover half of a wired pair. RTCM from the link UART into GNSS, then
// GGA/RMC (and optional GST) out. Loop: corrections first, then NMEA.
//
// ESP32: CRC-24Q assemble RTCM before writeRaw (partial UART bytes are not
// a valid frame). Then HostPump drains GNSS. AVR: ingestRaw + capped readLine.
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
#if defined(ARDUINO_ARCH_ESP32)
LC29H_UartPump::Pump uartPump;
LC29H_Rtcm::Assembler rtcmIn;
LC29H_Rtcm::Counters rtcmCounters;
#endif
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
    LC29H_HostPump::beginGnss(
        gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin, uartPump, LC29H_UartPump::roverGisPriorities());
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
    Serial.println("Mission: RTCM in first, then GGA+RMC out for GIS.");
    Serial.print("Local NMEA print=");
    Serial.println(kPrintLocalNmea ? "on" : "off");
    Serial.print("Forward NMEA to link=");
    Serial.println(kForwardNmeaToLink ? "on" : "off");
    Serial.println("Allowlist: GGA and RMC on (position and time).");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for RoverCorrectionBridge. Example stays disabled.");
        return;
    }

    LC29H_BringUpResult bringUp;
    roverEnabled = LC29H_bringUp(gnss, bringUp, &Serial);

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
#if defined(ARDUINO_ARCH_ESP32)
    // Assemble complete RTCM3 frames (CRC-24Q) before writeRaw. Partial BLE/UART
    // chunks are not valid messages; dumping them poisons the GNSS parser.
    while (correctionLinkPort.available() > 0) {
        const int b = correctionLinkPort.read();
        if (b < 0) {
            break;
        }
        rtcmIn.feed(static_cast<uint8_t>(b), gnss, &ingressStats, rtcmCounters);
    }
    LC29H_HostPump::tick(gnssPort, uartPump);
    Stream* nmeaDest = nullptr;
    if (kForwardNmeaToLink) {
        nmeaDest = &correctionLinkPort;
    } else if (kPrintLocalNmea) {
        nmeaDest = &Serial;
    }
    LC29H_HostPump::processTo(uartPump, nullptr, nmeaDest);
#else
    gnss.ingestRawAvailable(correctionLinkPort, 0, &ingressStats, kCorrectionChunkSize);
    gnssPort.listen();
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
        uint8_t n = 0;
        while (n < 8 && gnss.readLine(line, 0)) {
            Serial.println(line);
            ++n;
        }
    }
#endif

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printRawIngressStatus(Serial, "RoverCorrectionBridge", ingressStats, now);
        if (kForwardNmeaToLink) {
            LC29H_GNSS::printBridgeStatus(Serial, "RoverNmeaForward", bridgeMode, bridgeStats, now);
        }
        lastStatusMs = now;
    }
}