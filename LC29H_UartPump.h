#pragma once

#include <Arduino.h>
#include <string.h>

#include <LC29H_GNSS.h>

// Drain UART first (no parse), then frame into RTCM FIFO + latest-wins NMEA mailboxes.
// Priority 0/1/2 and aging limits are policy: the sketch fills PriorityTable.
//
// Level 0 = mission (RTCM frames). FIFO; drop oldest if full.
// Level 1 = needed status (RMC/GGA/SVIN). One mailbox each; newer overwrites unread.
// Level 2 = extras (GSV group, GSA, GST, EPE). Latest-wins. GSV ages after skipLimit
// skipped process cycles so sky view cannot starve forever, but it never preempts RTCM.

namespace LC29H_UartPump {

enum class Priority : uint8_t {
    Mission = 0,
    Needed = 1,
    Extra = 2
};

struct PriorityTable {
    Priority rmc = Priority::Needed;
    Priority gga = Priority::Needed;
    Priority svin = Priority::Needed;
    Priority gst = Priority::Extra;
    Priority gsa = Priority::Extra;
    Priority gsv = Priority::Extra;
    Priority epe = Priority::Extra;
    Priority other = Priority::Extra;
    // GSV group becomes due after this many process() calls while still pending.
    uint8_t gsvSkipLimit = 2;
};

inline PriorityTable baseStationPriorities() {
    return PriorityTable{};
}

inline PriorityTable roverGisPriorities() {
    PriorityTable t;
    t.rmc = Priority::Needed;
    t.gga = Priority::Needed;
    t.svin = Priority::Extra;
    t.gst = Priority::Extra;
    t.gsa = Priority::Extra;
    t.gsv = Priority::Extra;
    t.epe = Priority::Extra;
    t.gsvSkipLimit = 2;
    return t;
}

typedef void (*NmeaHandler)(const char* line, void* user);
typedef void (*RtcmHandler)(const uint8_t* frame, size_t len, void* user);

class Pump {
public:
    static constexpr size_t kDrainCap = 8192;
    static constexpr size_t kRtcmCap = 1100;
    static constexpr uint8_t kRtcmFifo = 6;
    static constexpr uint8_t kGsvMax = 16;
    static constexpr size_t kLineMax = LC29H_GNSS::kMaxBridgeNmeaChars;
    static constexpr uint32_t kDrainBudgetMs = 4;

    void setPriorities(const PriorityTable& table) { _pri = table; }
    const PriorityTable& priorities() const { return _pri; }

    // Copy bytes out of UART into the drain ring. No parse. tap is optional (traffic log).
    size_t drain(Stream& uart, Stream* tap = nullptr);
    // Split drain ring into RTCM FIFO + NMEA mailboxes. XOR-check NMEA here (no heap).
    void frame();
    // Level 0: RTCM FIFO, oldest first. Stops at budgetMs.
    uint8_t processRtcm(uint32_t budgetMs, RtcmHandler rtcm, void* user);
    // Level 1 then due-GSV then extras. budgetMs 0 still delivers needed status.
    uint8_t processNmea(uint32_t budgetMs, NmeaHandler nmea, void* user);
    // Deliver RTCM then NMEA until budgetMs. Convenience for sketches that do not split crumbs.
    uint8_t process(uint32_t budgetMs, RtcmHandler rtcm, NmeaHandler nmea, void* user);

    uint32_t drainOverruns() const { return _drainOverruns; }
    uint32_t drainBudgetHits() const { return _drainBudgetHits; }
    uint32_t checksumFails() const { return _checksumFails; }
    uint32_t mailboxOverwrites() const { return _overwrites; }
    uint32_t rtcmDrops() const { return _rtcmDrops; }
    uint32_t gsvDueCount() const { return _gsvDueCount; }
    uint32_t resyncs() const { return _resyncs; }
    uint32_t lastDrainBytes() const { return _lastDrainBytes; }
    uint8_t rtcmQueued() const { return _rtcmCount; }
    uint8_t gsvPendingLines() const { return _gsvPending ? _gsvCount : 0; }
    uint8_t gsvSkipped() const { return _gsvSkipped; }
    bool gsvDue() const { return _gsvDue; }
    uint16_t drainFill() const;

private:
    enum class Kind : uint8_t {
        Rmc,
        Gga,
        Svin,
        Gst,
        Gsa,
        Gsv,
        Epe,
        Other
    };

    struct LineSlot {
        char line[kLineMax] = {};
        bool pending = false;
        uint8_t skipped = 0;
    };

    struct RtcmSlot {
        uint16_t len = 0;
        uint8_t data[kRtcmCap] = {};
    };

    PriorityTable _pri{};
    uint8_t _drain[kDrainCap] = {};
    size_t _dHead = 0;
    size_t _dTail = 0;
    size_t _dCount = 0;

    uint8_t _nmeaAcc[kLineMax] = {};
    size_t _nmeaLen = 0;
    bool _inNmea = false;

    uint8_t _rtcmAcc[kRtcmCap] = {};
    size_t _rtcmLen = 0;
    size_t _rtcmExpected = 0;
    uint8_t _rtcmPhase = 0;

    RtcmSlot _rtcmQ[kRtcmFifo] = {};
    uint8_t _rtcmHead = 0;
    uint8_t _rtcmCount = 0;

    LineSlot _rmc{};
    LineSlot _gga{};
    LineSlot _svin{};
    LineSlot _gst{};
    LineSlot _gsa{};
    LineSlot _epe{};

    char _gsvLines[kGsvMax][kLineMax] = {};
    uint8_t _gsvCount = 0;
    bool _gsvPending = false;
    uint8_t _gsvSkipped = 0;
    bool _gsvDue = false;

    uint32_t _drainOverruns = 0;
    uint32_t _drainBudgetHits = 0;
    uint32_t _checksumFails = 0;
    uint32_t _overwrites = 0;
    uint32_t _rtcmDrops = 0;
    uint32_t _gsvDueCount = 0;
    uint32_t _resyncs = 0;
    uint32_t _lastDrainBytes = 0;

    void pushDrainByte(uint8_t b);
    bool popDrainByte(uint8_t& b);
    void onNmeaComplete();
    void onRtcmComplete();
    void postLine(Kind kind, const char* line);
    void postGsv(const char* line);
    static Kind classify(const char* line);
    static bool gsvIsFirst(const char* line);
    bool gsvSameTalkerRestart(const char* line) const;
    bool deliverSlot(LineSlot& slot, NmeaHandler nmea, void* user);
    uint8_t deliverGsv(NmeaHandler nmea, void* user);
};

}  // namespace LC29H_UartPump
