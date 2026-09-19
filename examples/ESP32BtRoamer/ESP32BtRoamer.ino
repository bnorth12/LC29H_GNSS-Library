#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <LC29H_MessageSchedule.h>
#include <LC29H_UartPump.h>
#include <LC29H_Rtcm.h>
#include <LC29H_NmeaCompat.h>
#include <string.h>

// Phone-app rover (SW Maps Generic NMEA over BLE NUS, SPP on classic ESP32).
//
// Why this is not a thin readLine() sketch:
// - LC29H NMEA+RTCM will overrun a 256-byte UART FIFO if loop() prints or
//   waits on USB CDC. Use UartPump drain/frame every tick.
// - DA rover mode needs RESTOREPAR → CFGRCVRMODE,W,1 → PAIR081,0 → rates →
//   SAVEPAR → PAIR023. SAVEPAR alone does not apply. Fitness nav blocks RTK.
// - BLE default MTU 23 splits GGA; SW Maps does not reassemble. Wait for
//   subscribe + large MTU, one complete sentence per notify.
// - DA has no GST and empty GGA DiffAge; synthesize GST from PQTMEPE.
//
// Boot: configure GNSS (amber LED) before advertising. Connect only on blue.
// Loop: RTCM from phone first (CRC-24Q), then pump NMEA, then staggered BLE TX.
// GETTING_STARTED.md and this folder's README.
//
// Hardware: ESP32 (SPP) or ESP32-S3 (BLE). GNSS UART pins in lc29hconfig.h.

#if !defined(ARDUINO_ARCH_ESP32)
#error "ESP32BtRoamer.ino is intended for ESP32 targets only."
#endif

#if defined(CONFIG_BT_SPP_ENABLED)
#define LC29H_BT_ROAMER_USE_SPP 1
#elif defined(CONFIG_NIMBLE_ENABLED) || defined(CONFIG_BLUEDROID_ENABLED)
#define LC29H_BT_ROAMER_USE_BLE 1
#else
#error "No supported Bluetooth transport found. Need either BT Classic SPP or BLE support."
#endif

#if defined(LC29H_BT_ROAMER_USE_SPP)
#include <BluetoothSerial.h>
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gatt.h>
#endif
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = LC29H_CFG_ESP32_BT_GNSS_BAUD;
constexpr uint32_t kStatusIntervalMs = 1000;

constexpr int kGnssRxPin = LC29H_CFG_ESP32_BT_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_CFG_ESP32_BT_GNSS_TX_PIN;
#if defined(LC29H_BT_ROAMER_USE_BLE)
constexpr size_t kBleNotifyChunkSize = 20;
constexpr size_t kBleNotifyMaxPayload = 180;
constexpr uint16_t kBleMinNmeaMtu = 100;
volatile uint16_t blePeerMtu = 23;
volatile bool bleNotifySubscribed = false;
volatile uint16_t bleConnHandle = 0xFFFF;
#endif

#if defined(LC29H_CFG_STATUS_RGB_PIN) && (LC29H_CFG_STATUS_RGB_PIN >= 0)
constexpr int kStatusRgbPin = LC29H_CFG_STATUS_RGB_PIN;
constexpr uint32_t kStatusLedFlashMs = 80;
constexpr uint8_t kLedIdleR = 8;
constexpr uint8_t kLedConnG = 20;
constexpr uint8_t kLedRxR = 48;
constexpr uint8_t kLedTxB = 48;
volatile uint32_t lastBtRxMs = 0;
volatile uint32_t lastBtTxMs = 0;

void noteBtRx() {
    lastBtRxMs = millis();
}

void updateStatusLed();

void noteBtTx() {
    lastBtTxMs = millis();
}
#else
inline void noteBtRx() {}
inline void noteBtTx() {}
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) && LC29H_CFG_DEBUG_UART0
HardwareSerial& dbg = Serial0;
#else
HardwareSerial& dbg = Serial;
#endif

void dbgReady() {
#if defined(CONFIG_IDF_TARGET_ESP32S3) && LC29H_CFG_DEBUG_UART0
    Serial0.begin(kConsoleBaud);
#endif
}

HardwareSerial& gnssPort = Serial2;
#if defined(LC29H_BT_ROAMER_USE_SPP)
BluetoothSerial btSerial;
#else
// Nordic UART Service UUIDs for BLE ingress from phone apps.
constexpr const char* kBleServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kBleRxCharUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kBleTxCharUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

class BleIngressStream : public Stream {
public:
    BleIngressStream() : _head(0), _tail(0), _dropped(0), _received(0) {}

    int available() override {
        return static_cast<int>((kBufferSize + _head - _tail) % kBufferSize);
    }

    int read() override {
        if (_head == _tail) {
            return -1;
        }
        const uint8_t value = _buffer[_tail];
        _tail = (_tail + 1) % kBufferSize;
        return static_cast<int>(value);
    }

    int peek() override {
        if (_head == _tail) {
            return -1;
        }
        return static_cast<int>(_buffer[_tail]);
    }

    void flush() override {}

    size_t write(uint8_t) override {
        return 1;
    }

    void push(const uint8_t* data, size_t len) {
        _received += static_cast<uint32_t>(len);
        for (size_t i = 0; i < len; ++i) {
            const size_t next = (_head + 1) % kBufferSize;
            if (next == _tail) {
                _dropped = _dropped + 1;
                continue;
            }
            _buffer[_head] = data[i];
            _head = next;
        }
    }

    uint32_t droppedBytes() const {
        return _dropped;
    }

    uint32_t receivedBytes() const {
        return _received;
    }

private:
    static constexpr size_t kBufferSize = 8192;
    uint8_t _buffer[kBufferSize];
    volatile size_t _head;
    volatile size_t _tail;
    volatile uint32_t _dropped;
    volatile uint32_t _received;
};

BleIngressStream bleIngress;
BLEServer* bleServer = nullptr;
BLECharacteristic* bleTxChar = nullptr;
bool bleClientConnected = false;

class RoverBleServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* server) override {
        (void)server;
        bleClientConnected = true;
        updateStatusLed();
        dbg.println("BLE client connected");
    }

#if defined(CONFIG_NIMBLE_ENABLED)
    void onConnect(BLEServer* server, ble_gap_conn_desc* desc) override {
        onConnect(server);
        if (desc != nullptr) {
            bleConnHandle = desc->conn_handle;
            ble_gattc_exchange_mtu(desc->conn_handle, nullptr, nullptr);
        }
    }

    void onMtuChanged(BLEServer* server, ble_gap_conn_desc* desc, uint16_t mtu) override {
        (void)server;
        (void)desc;
        blePeerMtu = mtu;
        dbg.print("BLE MTU=");
        dbg.println(mtu);
    }

    void onDisconnect(BLEServer* server, ble_gap_conn_desc* desc) override {
        (void)desc;
        onDisconnect(server);
    }
#endif

    void onDisconnect(BLEServer* server) override {
        bleClientConnected = false;
        bleNotifySubscribed = false;
        blePeerMtu = 23;
        bleConnHandle = 0xFFFF;
        updateStatusLed();
        if (server != nullptr) {
            server->startAdvertising();
        } else {
            BLEDevice::startAdvertising();
        }
        dbg.println("BLE client disconnected, advertising");
    }
};

class RoverBleTxCallbacks : public BLECharacteristicCallbacks {
public:
#if defined(CONFIG_NIMBLE_ENABLED)
    void onSubscribe(BLECharacteristic* characteristic, ble_gap_conn_desc* desc, uint16_t subValue) override {
        (void)characteristic;
        bleNotifySubscribed = (subValue & 0x0001) != 0;
        if (desc != nullptr) {
            bleConnHandle = desc->conn_handle;
        }
        dbg.print("BLE notify subscribed=");
        dbg.println(bleNotifySubscribed ? "yes" : "no");
    }
#endif
};

class RoverBleRxCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* characteristic) override {
        String value = characteristic->getValue();
        if (value.length() > 0) {
            bleIngress.push(reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
            noteBtRx();
        }
    }
};
#endif

#if LC29H_CFG_DEBUG_MIRROR_GNSS_UART
class DebugTeeStream : public Stream {
public:
    explicit DebugTeeStream(Stream& primary) : _primary(primary), _secondary(nullptr) {}

    void setSecondary(Stream* secondary) {
        _secondary = secondary;
    }

    size_t write(uint8_t c) override {
        _primary.write(c);
        if (_secondary != nullptr) {
            _secondary->write(c);
        }
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        _primary.write(buffer, size);
        if (_secondary != nullptr) {
            _secondary->write(buffer, size);
        }
        return size;
    }

    int available() override {
        return _primary.available();
    }

    int read() override {
        return _primary.read();
    }

    int peek() override {
        return _primary.peek();
    }

    void flush() override {
        _primary.flush();
        if (_secondary != nullptr) {
            _secondary->flush();
        }
    }

private:
    Stream& _primary;
    Stream* _secondary;
};

DebugTeeStream debugTee(Serial);
LC29H_GNSS gnss(gnssPort, &debugTee);
#else
LC29H_GNSS gnss(gnssPort, nullptr);
#endif
LC29H_GNSS::RawIngressStats correctionIngressStats;
LC29H_UartPump::Pump uartPump;
String holdGga;
String holdRmc;
String holdGst;
String holdGsa;
bool extraMsgsEnabled = false;
uint32_t lastGgaMs = 0;
uint32_t lastRmcMs = 0;
uint32_t lastGstMs = 0;
LC29H_Rtcm::Counters rtcmCounters;
LC29H_Rtcm::Assembler bleRtcmIn;
void noteModuleVersionLine(const String& line);

uint32_t nmeaLinesPrinted = 0;
uint32_t nmeaLinesSentToBt = 0;
uint32_t lastStatusMs = 0;
bool roverEnabled = false;
bool gnssStable = false;
bool bleReadyToAdvertise = false;
uint32_t gnssConfigStartMs = 0;

// Shared SPP+BLE: NMEA pump callback and module family (must not be BLE-only — classic ESP32 CI uses SPP).
LC29H_NmeaCompat::ModuleFamily moduleFamily = LC29H_NmeaCompat::ModuleFamily::Unknown;
String moduleVersion;
bool phoneRatesApplied = false;
bool versionQuerySent = false;

void noteModuleVersionLine(const String& line) {
    const LC29H_NmeaCompat::ModuleFamily parsed = LC29H_NmeaCompat::familyFromLine(line.c_str());
    if (parsed == LC29H_NmeaCompat::ModuleFamily::Unknown) {
        return;
    }
    moduleVersion = line;
    moduleFamily = parsed;
}

void onPumpedNmea(const char* line, void* user) {
    (void)user;
    if (line == nullptr || line[0] == '\0') {
        return;
    }
    ++nmeaLinesPrinted;
    noteModuleVersionLine(String(line));
    if (strstr(line, "GGA,") != nullptr) {
        holdGga = line;
        lastGgaMs = millis();
        if (!gnssStable) {
            gnssStable = true;
            dbg.println("GGA ok, GNSS stable");
        }
    } else if (strstr(line, "RMC,") != nullptr) {
        holdRmc = line;
        lastRmcMs = millis();
    } else if (strstr(line, "GST,") != nullptr) {
        holdGst = line;
        lastGstMs = millis();
    } else if (strstr(line, "GNGSA,") != nullptr) {
        holdGsa = line;
    } else if (strstr(line, "PQTMEPE") != nullptr) {
        char gst[160];
        if (LC29H_NmeaCompat::gstFromPqtmepe(line, holdGga.c_str(), gst, sizeof(gst))) {
            holdGst = gst;
            lastGstMs = millis();
        }
    }
}

#if defined(LC29H_BT_ROAMER_USE_SPP)
void sendNmeaLineToBt(const String& line) {
    btSerial.print(line);
    btSerial.print("\r\n");
    noteBtTx();
}
#else
bool nmeaLooksValid(const String& line) {
    return LC29H_GNSS::hasValidNmeaChecksum(line);
}

size_t bleNotifyPayloadSize() {
    uint16_t mtu = blePeerMtu;
    size_t payload = kBleNotifyChunkSize;
    if (mtu > 23) {
        payload = static_cast<size_t>(mtu - 3);
    }
    if (payload > kBleNotifyMaxPayload) {
        payload = kBleNotifyMaxPayload;
    }
    if (payload < kBleNotifyChunkSize) {
        payload = kBleNotifyChunkSize;
    }
    return payload;
}

void sendBtBleChunk(const uint8_t* data, size_t len) {
    if (!bleClientConnected || bleTxChar == nullptr || len == 0) {
        return;
    }
    bleTxChar->setValue(data, len);
    bleTxChar->notify();
}

struct BleNmeaGate {
    uint32_t intervalMs;
    uint32_t burstMs;
    uint8_t maxBurst;
    uint32_t windowStartMs;
    uint8_t sentInWindow;

    bool allow(uint32_t nowMs) {
        if (intervalMs == 0) {
            return true;
        }
        if (windowStartMs == 0 || (nowMs - windowStartMs) >= intervalMs) {
            windowStartMs = nowMs;
            sentInWindow = 1;
            return true;
        }
        if (burstMs > 0 && (nowMs - windowStartMs) <= burstMs && sentInWindow < maxBurst) {
            ++sentInWindow;
            return true;
        }
        return false;
    }
};

BleNmeaGate bleGgaGate{LC29H_CFG_BLE_NMEA_GGA_MS, 0, 1, 0, 0};
BleNmeaGate bleRmcGate{LC29H_CFG_BLE_NMEA_RMC_MS, 0, 1, 0, 0};
BleNmeaGate bleGstGate{LC29H_CFG_BLE_NMEA_GST_MS, 0, 1, 0, 0};

bool applyRoverPhoneNmeaRates() {
    const uint32_t fixMs = (LC29H_CFG_FIX_RATE_MS > 0) ? LC29H_CFG_FIX_RATE_MS : 1000;
    const uint8_t oneHz = LC29H_MessageSchedule::rateForPeriodMs(1000, fixMs);
    bool ok = true;
    ok = gnss.setMessageRate("GGA", 1, oneHz) && ok;
    ok = gnss.setMessageRate("RMC", 1, oneHz) && ok;
    ok = gnss.setMessageRate("GST", 1, oneHz) && ok;
    ok = gnss.setMessageRate("GSV", 1, 0) && ok;
    ok = gnss.setMessageRate("GSA", 1, 0) && ok;
    ok = gnss.setMessageRate("VTG", 1, 0) && ok;
    ok = gnss.setMessageRate("GLL", 1, 0) && ok;
    ok = gnss.setMessageRate("ZDA", 1, 0) && ok;
    ok = gnss.setMessageRate("GNS", 1, 0) && ok;
    ok = gnss.setMessageRate("GRS", 1, 0) && ok;
    return ok;
}

bool shouldForwardNmeaToBle(const String& line) {
    if (!nmeaLooksValid(line)) {
        return false;
    }
    const uint32_t nowMs = millis();
    if (line.indexOf("GGA,") >= 0) {
        return bleGgaGate.allow(nowMs);
    }
    if (line.indexOf("RMC,") >= 0) {
        return bleRmcGate.allow(nowMs);
    }
    if (line.indexOf("GST,") >= 0) {
        return bleGstGate.allow(nowMs);
    }
    return false;
}

void sendNmeaLineToBt(const String& line) {
    if (!bleClientConnected || bleTxChar == nullptr) {
        return;
    }
    if (!bleNotifySubscribed) {
        return;
    }
    noteBtTx();

    const size_t lineLen = line.length();
    if (lineLen < 6 || lineLen + 2 > kBleNotifyMaxPayload) {
        return;
    }
    uint8_t framed[kBleNotifyMaxPayload];
    memcpy(framed, line.c_str(), lineLen);
    framed[lineLen] = '\r';
    framed[lineLen + 1] = '\n';
    sendBtBleChunk(framed, lineLen + 2);
}
#endif

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

#if defined(LC29H_CFG_STATUS_RGB_PIN) && (LC29H_CFG_STATUS_RGB_PIN >= 0)
bool btClientIsConnected() {
#if defined(LC29H_BT_ROAMER_USE_SPP)
    return btSerial.hasClient();
#else
    return bleClientConnected;
#endif
}

void updateStatusLed() {
    const uint32_t now = millis();
    static uint32_t ledPhaseMs = 0;
    static bool ledOn = false;
    const uint32_t blinkMs = (!gnssStable) ? 200 : ((lastGgaMs != 0 && (now - lastGgaMs) > 3000) ? 120 : 700);
    if ((now - ledPhaseMs) >= blinkMs) {
        ledPhaseMs = now;
        ledOn = !ledOn;
    }
    const bool connected = btClientIsConnected();
    const bool rxFlash = (now - lastBtRxMs) < kStatusLedFlashMs;
    const bool txFlash = (now - lastBtTxMs) < kStatusLedFlashMs;

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    if (connected) {
        g = kLedConnG;
        if (lastGgaMs != 0 && (now - lastGgaMs) > 3000) {
            if (((now / 120) % 2) == 0) {
                r = 48;
            }
        } else if (rxFlash) {
            r = kLedRxR;
        }
        if (txFlash) {
            b = kLedTxB;
        }
    } else if (!gnssStable) {
        r = ledOn ? 40 : 8;
        g = ledOn ? 16 : 3;
    } else if (lastGgaMs != 0 && (now - lastGgaMs) > 3000) {
        r = ledOn ? 48 : 10;
    } else {
        b = ledOn ? 36 : 8;
    }
    rgbLedWrite(kStatusRgbPin, r, g, b);
}
#else
inline void updateStatusLed() {}
#endif

bool configureRoverAtBoot() {
    LC29H_ModuleIdentity id;
    const bool ok = LC29H_roverFactoryBringUp(gnss, &dbg, gnssPort, uartPump, updateStatusLed, &id);
    moduleFamily = id.family;
    extraMsgsEnabled = true;
    return ok;
}

#if LC29H_CFG_DEBUG_MIRROR_GNSS_UART
void debugMirrorGnssLine(const String& line) {
    Serial.println(line);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial0.println(line);
#endif
}

void debugMirrorGnssRx() {
    String line;
    while (gnss.readLine(line, 0)) {
        debugMirrorGnssLine(line);
        ++nmeaLinesPrinted;
    }
}
#endif
}

void setup() {
    Serial.begin(kConsoleBaud);
    // Do not call Serial.setTxTimeoutMs — absent on many esp32 core builds (CI Arduino CLI Validate).
    dbgReady();
    updateStatusLed();
    dbg.println("ESP32BtRoamer boot");

    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
    gnssPort.setTimeout(0);
    {
        LC29H_UartPump::PriorityTable pri = LC29H_UartPump::roverGisPriorities();
        pri.gst = LC29H_UartPump::Priority::Needed;
        pri.gsa = LC29H_UartPump::Priority::Needed;
        pri.epe = LC29H_UartPump::Priority::Needed;
        uartPump.setPriorities(pri);
    }
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());
    roverEnabled = true;
    dbg.println("GNSS UART up. Configuring rover (amber).");
    configureRoverAtBoot();
    gnssConfigStartMs = millis();
    dbg.println("Amber pulse = waiting GGA. Blue pulse = connect BLE.");

#if LC29H_CFG_DEBUG_MIRROR_GNSS_UART && defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial0.begin(kConsoleBaud);
    debugTee.setSecondary(&Serial0);
    Serial.println("GNSS UART debug mirror: USB Serial + UART0 (CH343)");
#elif LC29H_CFG_DEBUG_MIRROR_GNSS_UART
    Serial.println("GNSS UART debug mirror: USB Serial");
#endif

#if defined(LC29H_BT_ROAMER_USE_SPP)
    const char* btPin = LC29H_CFG_ESP32_BT_PIN;
    if (btPin[0] != '\0') {
        btSerial.setPin(btPin, static_cast<uint8_t>(strlen(btPin)));
    }

    if (!btSerial.begin(LC29H_CFG_ESP32_BT_NAME)) {
        Serial.println("Bluetooth SPP init failed.");
        return;
    }
#else
    BLEDevice::init(LC29H_CFG_ESP32_BT_NAME);
    BLEDevice::setMTU(517);
    bleServer = BLEDevice::createServer();
    if (bleServer == nullptr) {
        Serial.println("Bluetooth BLE server init failed.");
        return;
    }
    bleServer->setCallbacks(new RoverBleServerCallbacks());

    BLEService* service = bleServer->createService(kBleServiceUuid);
    if (service == nullptr) {
        Serial.println("Bluetooth BLE service init failed.");
        return;
    }

    BLECharacteristic* rxChar = service->createCharacteristic(
        kBleRxCharUuid,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    if (rxChar == nullptr) {
        Serial.println("Bluetooth BLE RX characteristic init failed.");
        return;
    }
    rxChar->setCallbacks(new RoverBleRxCallbacks());

    bleTxChar = service->createCharacteristic(
        kBleTxCharUuid,
        BLECharacteristic::PROPERTY_NOTIFY);
    if (bleTxChar == nullptr) {
        Serial.println("Bluetooth BLE TX characteristic init failed.");
        return;
    }
    bleTxChar->setCallbacks(new RoverBleTxCallbacks());

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    if (advertising == nullptr) {
        Serial.println("Bluetooth BLE advertising init failed.");
        return;
    }
    advertising->addServiceUUID(kBleServiceUuid);
    advertising->setScanResponse(true);
    // 30-50 ms connection interval is more stable with phones than 7.5 ms.
    advertising->setMinPreferred(0x18);
    advertising->setMaxPreferred(0x28);
    bleServer->advertiseOnDisconnect(true);
#endif

    Serial.println();
    Serial.println("ESP32BtRoamer example");
    Serial.println("Bluetooth correction ingress for phone-based RTIP/RTCM apps.");
    Serial.println("Use help registry, help bridge, help placeholders, and help family coreconfiguration in the library console.");
#if defined(LC29H_BT_ROAMER_USE_SPP)
    Serial.println("Transport mode: BT Classic SPP");
#else
    Serial.println("Transport mode: BLE (NUS RX characteristic)");
#endif
    Serial.print("Bluetooth device name: ");
    Serial.println(LC29H_CFG_ESP32_BT_NAME);
    Serial.print("GNSS UART: Serial2 RX=");
    Serial.print(kGnssRxPin);
    Serial.print(", TX=");
    Serial.println(kGnssTxPin);

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for ESP32BtRoamer. Example stays disabled.");
        roverEnabled = false;
        return;
    }

    Serial.println("Rover correction ingress active over Bluetooth.");
    updateStatusLed();
}

void loop() {
    updateStatusLed();
    gnss.processSerialCommands();
    updateStatusLed();

#if defined(LC29H_BT_ROAMER_USE_BLE)
    if (!gnssStable && gnssConfigStartMs != 0 && (millis() - gnssConfigStartMs) >= 12000) {
        gnssStable = true;
        dbg.println("GNSS wait timed out; advertising BLE anyway.");
    }
    if (gnssStable && !bleReadyToAdvertise) {
        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        if (advertising != nullptr) {
            advertising->start();
            bleReadyToAdvertise = true;
            dbg.println("GNSS stable. Connect BLE now (slow blue blink).");
        }
    }
    if (!bleClientConnected) {
        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        if (bleReadyToAdvertise && advertising != nullptr && !advertising->isAdvertising()) {
            advertising->start();
        }
    } else if (blePeerMtu < kBleMinNmeaMtu && bleConnHandle != 0xFFFF) {
        static uint32_t lastMtuExchangeMs = 0;
        const uint32_t nowMtu = millis();
        if ((nowMtu - lastMtuExchangeMs) >= 1000) {
#if defined(CONFIG_NIMBLE_ENABLED)
            ble_gattc_exchange_mtu(bleConnHandle, nullptr, nullptr);
#endif
            lastMtuExchangeMs = nowMtu;
        }
    }
#endif

#if defined(LC29H_BT_ROAMER_USE_SPP)
    const uint32_t rxBefore = correctionIngressStats.bytesRead;
    gnss.ingestRawAvailable(
        btSerial,
        0,
        &correctionIngressStats,
        LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE);
    if (correctionIngressStats.bytesRead > rxBefore) {
        noteBtRx();
    }
#else
    // Phone NTRIP arrives as BLE writes, not RTCM frames. Assemble + CRC before GNSS.
    while (bleIngress.available() > 0) {
        const int b = bleIngress.read();
        if (b < 0) {
            break;
        }
        const uint32_t okBefore = rtcmCounters.framesOk;
        bleRtcmIn.feed(static_cast<uint8_t>(b), gnss, &correctionIngressStats, rtcmCounters);
        if (rtcmCounters.framesOk > okBefore) {
            noteBtRx();
        }
    }
#endif

    uartPump.drain(gnssPort);
    uartPump.frame();
    uartPump.processNmea(8, onPumpedNmea, nullptr);  // checksum-good GGA/RMC/GSA/EPE only



    if (LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK != 0) {
        static uint32_t lastBleSlotMs = 0;
        static uint8_t bleSlot = 0;
        const uint32_t nowSlot = millis();
        // One sentence per 250 ms (GGA, RMC, GST, GSA) so SW Maps gets 1 Hz each
        // without a 4-notify clump that looks like a single burst then silence.
        if ((nowSlot - lastBleSlotMs) >= 250) {
            lastBleSlotMs = nowSlot;
            const String* slotLine = nullptr;
            if (bleSlot == 0) {
                slotLine = &holdGga;
            } else if (bleSlot == 1) {
                slotLine = &holdRmc;
            } else if (bleSlot == 2) {
                slotLine = &holdGst;
            } else {
                slotLine = &holdGsa;
            }
            bleSlot = static_cast<uint8_t>((bleSlot + 1) % 4);
            if (slotLine != nullptr && slotLine->length() > 0 &&
                LC29H_GNSS::hasValidNmeaChecksum(*slotLine)) {
                sendNmeaLineToBt(*slotLine);
                ++nmeaLinesSentToBt;
            }
        }
    }

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        dbg.print("ESP32BtRoamer: btBytesRead=");
        dbg.print(correctionIngressStats.bytesRead);
        dbg.print(", btBytesToGnss=");
        dbg.print(correctionIngressStats.bytesWritten);
        dbg.print(", shortWrites=");
        dbg.print(correctionIngressStats.shortWrites);
    #if defined(LC29H_BT_ROAMER_USE_BLE)
        dbg.print(", btIngressDropped=");
        dbg.print(bleIngress.droppedBytes());
        dbg.print(", btIngressReceived=");
        dbg.print(bleIngress.receivedBytes());
    #endif
        dbg.print(", localNmeaLines=");
        dbg.print(nmeaLinesPrinted);
        dbg.print(", btNmeaLines=");
        dbg.print(nmeaLinesSentToBt);
        dbg.print(", btClientConnected=");
    #if defined(LC29H_BT_ROAMER_USE_SPP)
        dbg.print(btSerial.hasClient() ? "yes" : "no");
    #else
        dbg.print(bleClientConnected ? "yes" : "no");
        dbg.print(", blePeers=");
        dbg.print((bleServer != nullptr) ? bleServer->getConnectedCount() : 0);
        dbg.print(", bleMtu=");
        dbg.print(blePeerMtu);
        dbg.print(", bleSub=");
        dbg.print(bleNotifySubscribed ? "yes" : "no");
        dbg.print(", module=");
        dbg.print(LC29H_NmeaCompat::familyName(moduleFamily));
        dbg.print(", gnssOk=");
        dbg.print((lastGgaMs != 0 && (now - lastGgaMs) < 3000) ? "yes" : "no");
        dbg.print(", bleTxOk=");
        dbg.print((lastBtTxMs != 0 && (now - lastBtTxMs) < 2000) ? "yes" : "no");
        dbg.print(", bleRxOk=");
        dbg.print((lastBtRxMs != 0 && (now - lastBtRxMs) < 5000) ? "yes" : "no");
        dbg.print(", nmeaCkFail=");
        dbg.print(uartPump.checksumFails());
        dbg.print(", drainOvf=");
        dbg.print(uartPump.drainOverruns());
        dbg.print(", rtcmOk=");
        dbg.print(rtcmCounters.framesOk);
        dbg.print(", rtcmCrcFail=");
        dbg.print(rtcmCounters.crcFails);
        dbg.print(", t1005=");
        dbg.print(rtcmCounters.msg1005);
        dbg.print(", t1006=");
        dbg.print(rtcmCounters.msg1006);
        dbg.print(", msm4=");
        dbg.print(rtcmCounters.msm4);
        dbg.print(", msm7=");
        dbg.print(rtcmCounters.msm7);
        dbg.print(", lastRtcm=");
        dbg.print(rtcmCounters.lastType);
    #endif
        dbg.print(", uptimeMs=");
        dbg.println(now);
        lastStatusMs = now;
    }
}
