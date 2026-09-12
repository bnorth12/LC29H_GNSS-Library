#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <LC29H_ProjectConfig.h>
#include <LC29H_UartPump.h>

// Shared ESP32 example host: GNSS UART is one reader. Drain the driver FIFO
// every loop tick, then frame. Do not use unbounded readLine() on ESP32.

namespace LC29H_HostPump {

inline void beginGnss(
    HardwareSerial& port,
    uint32_t baud,
    int rxPin,
    int txPin,
    LC29H_UartPump::Pump& pump,
    LC29H_UartPump::PriorityTable pri) {
    LC29H_beginEsp32GnssUart(port, baud, rxPin, txPin);
    port.setTimeout(0);  // readBytes must not block the 4 ms drain budget
    pump.setPriorities(pri);
}

inline void tick(HardwareSerial& port, LC29H_UartPump::Pump& pump) {
    pump.drain(port);
    pump.frame();
}

inline void printNmeaLine(const char* line, void* user) {
    if (line != nullptr && user != nullptr) {
        static_cast<Stream*>(user)->println(line);
    }
}

inline void writeRtcm(const uint8_t* data, size_t len, void* user) {
    if (data != nullptr && len > 0 && user != nullptr) {
        static_cast<Stream*>(user)->write(data, len);
    }
}

inline void processTo(
    LC29H_UartPump::Pump& pump,
    Stream* rtcmDest,
    Stream* nmeaDest,
    uint32_t rtcmBudgetMs = 8,
    uint32_t nmeaBudgetMs = 8) {
    if (rtcmDest != nullptr) {
        pump.processRtcm(rtcmBudgetMs, writeRtcm, rtcmDest);
    }
    if (nmeaDest != nullptr) {
        pump.processNmea(nmeaBudgetMs, printNmeaLine, nmeaDest);
    } else {
        pump.discardNmeaMailboxes();
    }
}

}  // namespace LC29H_HostPump

#endif
