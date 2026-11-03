#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <LC29H_ProjectConfig.h>
#include <LC29H_UartPump.h>
#include <string.h>

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

// Fixed-period poll: call every loop pass. Do not wrap this in delay().
// The Arduino loop task is the pump task. No second FreeRTOS task, so the
// core loopTask watchdog already covers it.
inline void tick(HardwareSerial& port, LC29H_UartPump::Pump& pump) {
    pump.drain(port);
    pump.frame();
}

inline uint32_t& txDropCount() {
    static uint32_t drops = 0;
    return drops;
}

inline LC29H_UartPump::PumpHealth snapshotHealth(const LC29H_UartPump::Pump& pump) {
    static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
    const LC29H_UartPump::PumpHealth health = pump.pumpHealth();
    portEXIT_CRITICAL(&mux);
    return health;
}

// Drop the whole payload when the sink cannot take it. Do not block in write.
inline bool writeWholeOrDrop(Stream& out, const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return true;
    }
    const int space = out.availableForWrite();
    if (space < static_cast<int>(len)) {
        ++txDropCount();
        return false;
    }
    const size_t wrote = out.write(data, len);
    if (wrote != len) {
        ++txDropCount();
        return false;
    }
    return true;
}

inline void printNmeaLine(const char* line, void* user) {
    if (line == nullptr || user == nullptr) {
        return;
    }
    const size_t len = strlen(line);
    if (len + 2 > 240) {
        ++txDropCount();
        return;
    }
    uint8_t framed[242];
    memcpy(framed, line, len);
    framed[len] = '\r';
    framed[len + 1] = '\n';
    writeWholeOrDrop(*static_cast<Stream*>(user), framed, len + 2);
}

inline void writeRtcm(const uint8_t* data, size_t len, void* user) {
    if (user == nullptr) {
        return;
    }
    writeWholeOrDrop(*static_cast<Stream*>(user), data, len);
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
