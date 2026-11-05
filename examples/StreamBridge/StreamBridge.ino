#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <lc29h_board.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_HostPump.h>
#endif

// Forwards GNSS output to an upstream UART for a caster or second board.
// RTCM is the mission stream. LC29H_bringUp() adopts a matching live SVIN or
// starts survey-in, then schedules status NMEA. Pump every loop.
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
HardwareSerial& linkPort = Serial2;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
constexpr int kLinkRxPin = 6;
constexpr int kLinkTxPin = 5;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
SoftwareSerial linkPort(kLinkRxPin, kLinkTxPin);
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
    linkPort.begin(kLinkBaud, SERIAL_8N1, kLinkRxPin, kLinkTxPin);
#else
    gnssPort.begin(kGnssBaud);
    linkPort.begin(kLinkBaud);
    gnssPort.listen();
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("StreamBridge example");
    Serial.println("RTCM 1 Hz mission. Adopt live SVIN if MinDur matches. Status NMEA scheduled.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("Bridge examples require lc29hconfig.h. Example stays disabled.");
        return;
    }

    
#if defined(ARDUINO_ARCH_ESP32)
    // Settle + drain before PQTM identity (live NMEA after prior sketch / BS rates).
    delay(300);
    while (gnssPort.available() > 0) { (void)gnssPort.read(); }
#endif

    // Issue #3 A / Reviewer B2: gate BEFORE bringUp so rover HIL hits wrong_mode
    // without bringUp rewriting CFGRCVRMODE to base and racing the post-reboot query.
    uint8_t receiverMode = 0;
    const bool modeOk = gnss.getReceiverMode(receiverMode);
    if (!modeOk) {
        Serial.println("HIL_MODE=QUERY_FAIL");
        Serial.println("HIL_EXAMPLE=StreamBridge STATUS=FAIL mode_query");
        bridgeEnabled = false;
    } else if (receiverMode != 2) {
        Serial.print("HIL_MODE=");
        Serial.println(receiverMode);
        Serial.println("HIL_EXAMPLE=StreamBridge STATUS=FAIL wrong_mode_need_base2");
        bridgeEnabled = false;
    } else {
        LC29H_BringUpResult bringUp;
        bridgeEnabled = LC29H_bringUp(gnss, bringUp, &Serial);
        if (bridgeEnabled) {
#if defined(ARDUINO_ARCH_ESP32)
            gnssPort.setTimeout(0);
#endif
            Serial.print("HIL_MODE=");
            Serial.println(receiverMode);
            Serial.println("HIL_EXAMPLE=StreamBridge STATUS=READY");
            Serial.println("NOTE: SAVEPAR may leave module non-default; restore BS rates after HIL.");
        } else {
            Serial.println("HIL_EXAMPLE=StreamBridge STATUS=FAIL bringup");
        }
    }

    bridgeMode = LC29H_projectBridgeMode();
    bridgeFilter = LC29H_projectBridgeNmeaFilter();
    bridgeFilterEnabled = LC29H_projectBridgeNmeaFilterEnabled();
    debugMode = LC29H_projectLocalDebugOutputMode();

    Serial.print("Bridge mode=");
    Serial.println(LC29H_GNSS::bridgeModeName(bridgeMode));
    Serial.print("Local debug output=");
    Serial.println(LC29H_GNSS::localDebugOutputModeName(debugMode));
    Serial.println("Bridge allowlist defaults: GGA on, GST off, RMC off, PQTM off.");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");
    Serial.println(bridgeEnabled ? "Bridge active." : "Bridge startup failed.");
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
    // Mission bytes (RTCM) to linkPort; NMEA only if debug mode is NmeaOnly.
    LC29H_HostPump::tick(gnssPort, uartPump);
    LC29H_HostPump::processTo(uartPump, &linkPort, localNmeaOut);
#else
    gnss.forwardBridgeAvailable(
        linkPort,
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
        LC29H_GNSS::printBridgeStatus(Serial, "StreamBridge", bridgeMode, bridgeStats, now);
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