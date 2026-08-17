#include <Arduino.h>

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
    Serial.println(F("Raw passthrough: USB Serial <-> GNSS UART"));
    Serial.println(F("Bridge guidance now lives in the main library help: registry, bridge, placeholders, and family summaries."));
}

void loop() {
    while (gnssPort.available() > 0) {
        Serial.write(static_cast<uint8_t>(gnssPort.read()));
    }

    while (Serial.available() > 0) {
        gnssPort.write(static_cast<uint8_t>(Serial.read()));
    }
}
