#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <string.h>

// Minimum verified hardware:
// - ESP32 with Bluetooth support (SPP preferred, BLE fallback)
// - GNSS module connected to configured UART pins

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
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = LC29H_CFG_ESP32_BT_GNSS_BAUD;
constexpr uint32_t kStatusIntervalMs = 1000;

constexpr int kGnssRxPin = LC29H_CFG_ESP32_BT_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_CFG_ESP32_BT_GNSS_TX_PIN;
#if defined(LC29H_BT_ROAMER_USE_BLE)
constexpr size_t kBleNotifyChunkSize = 20;
#endif

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
    static constexpr size_t kBufferSize = 2048;
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
    }

    void onDisconnect(BLEServer* server) override {
        (void)server;
        bleClientConnected = false;
        BLEDevice::startAdvertising();
    }
};

class RoverBleRxCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* characteristic) override {
        String value = characteristic->getValue();
        if (value.length() > 0) {
            bleIngress.push(reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
        }
    }
};
#endif

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::RawIngressStats correctionIngressStats;

uint32_t nmeaLinesPrinted = 0;
uint32_t nmeaLinesSentToBt = 0;
uint32_t lastStatusMs = 0;
bool roverEnabled = false;

#if defined(LC29H_BT_ROAMER_USE_SPP)
void sendNmeaLineToBt(const String& line) {
    btSerial.print(line);
    btSerial.print("\r\n");
}
#else
void sendBtBleChunk(const uint8_t* data, size_t len) {
    if (!bleClientConnected || bleTxChar == nullptr || len == 0) {
        return;
    }
    bleTxChar->setValue(data, len);
    bleTxChar->notify();
}

void sendNmeaLineToBt(const String& line) {
    if (!bleClientConnected || bleTxChar == nullptr) {
        return;
    }

    const char* text = line.c_str();
    size_t remaining = line.length();
    while (remaining > 0) {
        const size_t chunk = (remaining > kBleNotifyChunkSize) ? kBleNotifyChunkSize : remaining;
        sendBtBleChunk(reinterpret_cast<const uint8_t*>(text), chunk);
        text += chunk;
        remaining -= chunk;
    }

    static const char kCrLf[] = "\r\n";
    sendBtBleChunk(reinterpret_cast<const uint8_t*>(kCrLf), 2);
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
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

    gnssPort.begin(kGnssBaud, SERIAL_8N1, kGnssRxPin, kGnssTxPin);

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
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    if (bleTxChar == nullptr) {
        Serial.println("Bluetooth BLE TX characteristic init failed.");
        return;
    }

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    if (advertising == nullptr) {
        Serial.println("Bluetooth BLE advertising init failed.");
        return;
    }
    advertising->addServiceUUID(kBleServiceUuid);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

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
        return;
    }

    LC29H_GNSS::ProfileResult profileResult{
        LC29H_GNSS::ProfileStatus::CommandFailed,
        false,
    };
    roverEnabled = LC29H_applyProjectConfig(gnss, profileResult) &&
        profileResult.status == LC29H_GNSS::ProfileStatus::Success;

    Serial.print("Project config status=");
    Serial.println(profileStatusName(profileResult.status));
    if (profileResult.powerCycleRecommended) {
        Serial.println("Power cycle recommended after configuration.");
    }

    if (!roverEnabled) {
        Serial.println("Rover setup failed.");
        return;
    }

    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.queryFixRate();

    Serial.println("Rover correction ingress active over Bluetooth.");
}

void loop() {
    gnss.processSerialCommands();

    if (!roverEnabled) {
        return;
    }

#if defined(LC29H_BT_ROAMER_USE_SPP)
    gnss.ingestRawAvailable(
        btSerial,
        0,
        &correctionIngressStats,
        LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE);
#else
    gnss.ingestRawAvailable(
        bleIngress,
        0,
        &correctionIngressStats,
        LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE);
#endif

    if (LC29H_CFG_ROVER_PRINT_LOCAL_NMEA != 0 || LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK != 0) {
        String line;
        while (gnss.readLine(line, 0)) {
            if (LC29H_CFG_ROVER_PRINT_LOCAL_NMEA != 0) {
                Serial.println(line);
                ++nmeaLinesPrinted;
            }
            if (LC29H_CFG_ROVER_FORWARD_NMEA_TO_LINK != 0) {
                sendNmeaLineToBt(line);
                ++nmeaLinesSentToBt;
            }
        }
    }

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        Serial.print("ESP32BtRoamer: btBytesRead=");
        Serial.print(correctionIngressStats.bytesRead);
        Serial.print(", btBytesToGnss=");
        Serial.print(correctionIngressStats.bytesWritten);
        Serial.print(", shortWrites=");
        Serial.print(correctionIngressStats.shortWrites);
    #if defined(LC29H_BT_ROAMER_USE_BLE)
        Serial.print(", btIngressDropped=");
        Serial.print(bleIngress.droppedBytes());
        Serial.print(", btIngressReceived=");
        Serial.print(bleIngress.receivedBytes());
    #endif
        Serial.print(", localNmeaLines=");
        Serial.print(nmeaLinesPrinted);
        Serial.print(", btNmeaLines=");
        Serial.print(nmeaLinesSentToBt);
        Serial.print(", btClientConnected=");
    #if defined(LC29H_BT_ROAMER_USE_SPP)
        Serial.print(btSerial.hasClient() ? "yes" : "no");
    #else
        Serial.print(bleClientConnected ? "yes" : "no");
    #endif
        Serial.print(", uptimeMs=");
        Serial.println(now);
        lastStatusMs = now;
    }
}
