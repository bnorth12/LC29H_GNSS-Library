#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <lc29h_board.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_HostPump.h>
#endif

// Bench base pair: parse GNSS UART and forward RTCM to a rover on a second UART.
// LC29H_bringUp() adopts a matching live SVIN or starts survey-in. RTCM stays 1 Hz.
// Pump every loop; do not parse inside the pump callback.
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

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = LC29H_BOARD_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_BOARD_GNSS_TX_PIN;
constexpr int kLinkRxPin = LC29H_BOARD_LINK_RX_PIN;
constexpr int kLinkTxPin = LC29H_BOARD_LINK_TX_PIN;
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
#if defined(ARDUINO_ARCH_ESP32)
LC29H_UartPump::Pump uartPump;
#endif
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
    uartPump.setPriorities(LC29H_UartPump::baseStationPriorities());  // setTimeout(0) after bring-up
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

    
#if defined(ARDUINO_ARCH_ESP32)
    // Settle + drain before PQTM identity (live NMEA after prior sketch / BS rates).
    delay(300);
    while (gnssPort.available() > 0) { (void)gnssPort.read(); }
#endif

    LC29H_BringUpResult bringUp;
    bridgeEnabled = LC29H_bringUp(gnss, bringUp, &Serial);
    if (bridgeEnabled) {
#if defined(ARDUINO_ARCH_ESP32)
        gnssPort.setTimeout(0);
#endif
        Serial.println("HIL_EXAMPLE=BaseSerialBridge STATUS=READY");
        Serial.println("NOTE: SAVEPAR may leave module non-default; restore BS rates after HIL.");
    } else {
        Serial.println("HIL_EXAMPLE=BaseSerialBridge STATUS=FAIL bringup");
    }

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

#if defined(ARDUINO_ARCH_ESP32)
    // RTCM to the rover UART is the mission stream; NMEA is optional debug.
    LC29H_HostPump::tick(gnssPort, uartPump);
    LC29H_HostPump::processTo(uartPump, &roverLinkPort, localNmeaOut);
#else
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
#endif

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        LC29H_GNSS::printBridgeStatus(Serial, "BaseSerialBridge", bridgeMode, bridgeStats, now);
#if defined(ARDUINO_ARCH_ESP32)
        const LC29H_UartPump::PumpHealth health = LC29H_HostPump::snapshotHealth(uartPump);
        Serial.print("pump frames=");
        Serial.print(health.frames);
        Serial.print(" mailboxOverwrites=");
        Serial.print(health.drops);
        Serial.print(" usbCdcBackpressureDrops=");
        Serial.print(LC29H_HostPump::txDropCount());
        Serial.print(" high=");
        Serial.println(health.drainHighWater);
#endif
        lastStatusMs = now;
    }
}