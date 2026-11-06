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

#if defined(ARDUINO_ARCH_ESP32)
// HIL: count HostPump mission writes to linkPort (ESP32 path does not update bridgeStats).
class LinkHilStream : public Stream {
public:
    explicit LinkHilStream(Stream& inner) : _inner(inner) {}
    int available() override { return _inner.available(); }
    int read() override { return _inner.read(); }
    int peek() override { return _inner.peek(); }
    void flush() override { _inner.flush(); }
    int availableForWrite() override { return _inner.availableForWrite(); }
    size_t write(uint8_t b) override {
        const size_t n = _inner.write(b);
        if (n) {
            _bytes += n;
            feed(b);
        }
        return n;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        const size_t n = _inner.write(buffer, size);
        for (size_t i = 0; i < n; ++i) {
            feed(buffer[i]);
        }
        _bytes += n;
        return n;
    }
    uint32_t bytesOut() const { return _bytes; }
    uint32_t rtcmOk() const { return _rtcmOk; }
    uint32_t rtcmCrcFail() const { return _rtcmCrcFail; }
    uint32_t rtcmResync() const { return _rtcmResync; }

private:
    Stream& _inner;
    uint32_t _bytes = 0;
    uint32_t _rtcmOk = 0;
    uint32_t _rtcmCrcFail = 0;
    uint32_t _rtcmResync = 0;
    uint8_t _st = 0;
    uint16_t _len = 0;
    uint16_t _got = 0;
    uint8_t _hdr[3];
    uint8_t _body[1024];
    uint32_t _crc = 0;

    static uint32_t crc24q(const uint8_t* data, size_t len) {
        uint32_t crc = 0;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint32_t>(data[i]) << 16;
            for (int bit = 0; bit < 8; ++bit) {
                crc <<= 1;
                if (crc & 0x1000000u) {
                    crc ^= 0x1864CFBu;
                }
            }
        }
        return crc & 0xFFFFFFu;
    }

    void feed(uint8_t b) {
        if (_st == 0) {
            if (b == 0xD3) {
                _hdr[0] = b;
                _st = 1;
            }
            return;
        }
        if (_st == 1) {
            _hdr[1] = b;
            _st = 2;
            return;
        }
        if (_st == 2) {
            _hdr[2] = b;
            _len = (static_cast<uint16_t>(_hdr[1] & 0x03) << 8) | _hdr[2];
            if (_len > 1023) {
                ++_rtcmResync;
                _st = 0;
                return;
            }
            _got = 0;
            _st = 3;
            return;
        }
        if (_st == 3) {
            if (_got < _len) {
                _body[_got++] = b;
                return;
            }
            if (_got == _len) {
                _crc = static_cast<uint32_t>(b) << 16;
                ++_got;
                return;
            }
            if (_got == (_len + 1)) {
                _crc |= static_cast<uint32_t>(b) << 8;
                ++_got;
                return;
            }
            _crc |= b;
            uint8_t framed[3 + 1024];
            framed[0] = _hdr[0];
            framed[1] = _hdr[1];
            framed[2] = _hdr[2];
            for (uint16_t i = 0; i < _len; ++i) {
                framed[3 + i] = _body[i];
            }
            const uint32_t calc = crc24q(framed, static_cast<size_t>(3 + _len));
            if (calc == _crc) {
                ++_rtcmOk;
            } else {
                ++_rtcmCrcFail;
            }
            _st = 0;
        }
    }
};

LinkHilStream linkHil(linkPort);
#endif
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
    LC29H_HostPump::processTo(uartPump, &linkHil, localNmeaOut);
        // HIL link-port egress (HostPump path; bridgeStats stay 0 on ESP32).
        static uint32_t lastLinkHilMs = 0;
        if ((millis() - lastLinkHilMs) >= 1000) {
            lastLinkHilMs = millis();
            Serial.print("HIL_LINK_BYTES=");
            Serial.print(linkHil.bytesOut());
            Serial.print(" HIL_LINK_RTCM_OK=");
            Serial.print(linkHil.rtcmOk());
            Serial.print(" HIL_LINK_RTCM_CRC_FAIL=");
            Serial.print(linkHil.rtcmCrcFail());
            Serial.print(" HIL_LINK_RTCM_RESYNC=");
            Serial.println(linkHil.rtcmResync());
        }
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