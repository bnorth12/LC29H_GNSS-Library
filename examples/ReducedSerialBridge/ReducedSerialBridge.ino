#include <Arduino.h>
#include <LC29H_NmeaFeed.h>

// Raw USB Serial <-> GNSS UART passthrough. No LC29H_GNSS object.
// One reader on each side. Do not also open QGNSS on this same UART.
// Message payloads: this folder's README and library Readme "Module messages in practice".

#if defined(ARDUINO_ARCH_AVR) && !defined(HAVE_HWSERIAL1)
#include <SoftwareSerial.h>
SoftwareSerial gnssPort(4, 3); // RX, TX
#else
#define gnssPort Serial1
#endif

static const uint32_t kConsoleBaud = 115200;
static const uint32_t kGnssBaud = 115200;

void setup() {
    Serial.begin(kConsoleBaud);
    gnssPort.begin(kGnssBaud);

    Serial.println(F("ReducedSerialBridge ready."));
    Serial.println(F("Raw passthrough: USB Serial <-> GNSS UART (one reader on each side)."));
    Serial.println(F("Do not mix this sketch with QGNSS or a second UART client on the same port."));
}

static uint32_t usbDrops = 0;
static uint32_t gnssDrops = 0;

static bool writeByteOrDrop(Stream& out, uint8_t byte, uint32_t& drops) {
#if defined(ARDUINO_ARCH_ESP32)
    if (out.availableForWrite() <= 0) {
        ++drops;
        return false;
    }
#else
    (void)drops;
#endif
    out.write(byte);
    return true;
}

void loop() {
    // Cap per tick so USB Serial.print cannot run until the GNSS FIFO overflows.
    // No delay() here. A full TX buffer drops the byte already pulled from the source.
    uint8_t n = 0;
    static uint32_t nmeaFed = 0;
    while (n < 64 && gnssPort.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(gnssPort.read());
        writeByteOrDrop(Serial, b, usbDrops);
        nmeaFed += LC29H_NmeaFeed::feedNmeaBytes(&b, 1, [](uint8_t, void*) {}, nullptr);
        ++n;
    }
    (void)nmeaFed;

    n = 0;
    while (n < 64 && Serial.available() > 0) {
        const int raw = Serial.read();
        if (raw < 0) {
            break;
        }
        writeByteOrDrop(gnssPort, static_cast<uint8_t>(raw), gnssDrops);
        ++n;
    }

    static uint32_t lastPrint = 0;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastPrint) >= 2000) {
        lastPrint = now;
        Serial.print("usbDrops=");
        Serial.print(usbDrops);
        Serial.print(" gnssDrops=");
        Serial.println(gnssDrops);
    }
}
