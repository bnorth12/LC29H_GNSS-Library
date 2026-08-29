#if defined(ARDUINO_ARCH_ESP32)

#include "LC29H_UartPump.h"

namespace LC29H_UartPump {

uint16_t Pump::drainFill() const {
    return static_cast<uint16_t>(_dCount);
}

void Pump::pushDrainByte(uint8_t b) {
    if (_dCount >= kDrainCap) {
        _dTail = (_dTail + 1U) % kDrainCap;
        --_dCount;
        ++_drainOverruns;
    }
    _drain[_dHead] = b;
    _dHead = (_dHead + 1U) % kDrainCap;
    ++_dCount;
}

bool Pump::popDrainByte(uint8_t& b) {
    if (_dCount == 0) {
        return false;
    }
    b = _drain[_dTail];
    _dTail = (_dTail + 1U) % kDrainCap;
    --_dCount;
    return true;
}

size_t Pump::drain(Stream& uart, Stream* tap) {
    uint8_t tmp[128];
    size_t total = 0;
    const uint32_t started = millis();
    while (uart.available() > 0) {
        if ((millis() - started) >= kDrainBudgetMs) {
            ++_drainBudgetHits;
            break;
        }
        int avail = uart.available();
        if (avail <= 0) {
            break;
        }
        size_t want = static_cast<size_t>(avail);
        if (want > sizeof(tmp)) {
            want = sizeof(tmp);
        }
        size_t got = 0;
        while (got < want) {
            const int c = uart.read();
            if (c < 0) {
                break;
            }
            tmp[got++] = static_cast<uint8_t>(c);
        }
        if (got == 0) {
            break;
        }
        for (size_t i = 0; i < got; ++i) {
            pushDrainByte(tmp[i]);
        }
        if (tap != nullptr) {
            tap->write(tmp, got);
        }
        total += got;
    }
    _lastDrainBytes = static_cast<uint32_t>(total);
    return total;
}

Pump::Kind Pump::classify(const char* line) {
    if (line == nullptr || line[0] != '$' || line[1] == '\0') {
        return Kind::Other;
    }
    if (strncmp(line + 1, "PQTMSVINSTATUS", 14) == 0) {
        return Kind::Svin;
    }
    if (strncmp(line + 1, "PQTMEPE", 7) == 0) {
        return Kind::Epe;
    }
    if (strlen(line) < 6) {
        return Kind::Other;
    }
    const char* fmt = line + 3;
    if (strncmp(fmt, "RMC", 3) == 0) {
        return Kind::Rmc;
    }
    if (strncmp(fmt, "GGA", 3) == 0) {
        return Kind::Gga;
    }
    if (strncmp(fmt, "GST", 3) == 0) {
        return Kind::Gst;
    }
    if (strncmp(fmt, "GSA", 3) == 0) {
        return Kind::Gsa;
    }
    if (strncmp(fmt, "GSV", 3) == 0) {
        return Kind::Gsv;
    }
    return Kind::Other;
}

bool Pump::gsvIsFirst(const char* line) {
    // $GPGSV,<total>,<num>,...  first sentence of a group has <num> == 1
    const char* p = strchr(line, ',');
    if (p == nullptr) {
        return false;
    }
    p = strchr(p + 1, ',');
    if (p == nullptr) {
        return false;
    }
    ++p;
    while (*p == '0') {
        ++p;
    }
    return *p == '1' && (p[1] == ',' || p[1] == '*' || p[1] == '\0');
}

bool Pump::gsvSameTalkerRestart(const char* line) const {
    // Each constellation emits its own $xxGSV,...,1. That is not a new epoch.
    // A new epoch starts when a talker we already collected starts again.
    if (!gsvIsFirst(line) || _gsvCount == 0 || line[1] == '\0' || line[2] == '\0') {
        return false;
    }
    const char t1 = line[1];
    const char t2 = line[2];
    for (uint8_t i = 0; i < _gsvCount; ++i) {
        if (_gsvLines[i][1] == t1 && _gsvLines[i][2] == t2) {
            return true;
        }
    }
    return false;
}

void Pump::postLine(Kind kind, const char* line) {
    LineSlot* slot = nullptr;
    switch (kind) {
    case Kind::Rmc:
        slot = &_rmc;
        break;
    case Kind::Gga:
        slot = &_gga;
        break;
    case Kind::Svin:
        slot = &_svin;
        break;
    case Kind::Gst:
        slot = &_gst;
        break;
    case Kind::Gsa:
        slot = &_gsa;
        break;
    case Kind::Epe:
        slot = &_epe;
        break;
    default:
        return;
    }
    if (slot->pending) {
        ++_overwrites;
    }
    strncpy(slot->line, line, kLineMax - 1);
    slot->line[kLineMax - 1] = '\0';
    slot->pending = true;
}

void Pump::postGsv(const char* line) {
    if (gsvSameTalkerRestart(line)) {
        // New GSV group while the previous group is still unread: latest-wins, and that
        // overwrite is one missed sky-view group (~20 s at RATE 20). Age after skipLimit
        // missed groups so GSV can rise after RTCM+needed, never to level 0.
        if (_gsvPending) {
            ++_overwrites;
            if (_gsvSkipped < 255) {
                ++_gsvSkipped;
            }
            if (_gsvSkipped >= _pri.gsvSkipLimit) {
                if (!_gsvDue) {
                    ++_gsvDueCount;
                }
                _gsvDue = true;
            }
        }
        _gsvCount = 0;
        _gsvPending = false;
    }
    if (_gsvCount >= kGsvMax) {
        ++_overwrites;
        return;
    }
    strncpy(_gsvLines[_gsvCount], line, kLineMax - 1);
    _gsvLines[_gsvCount][kLineMax - 1] = '\0';
    ++_gsvCount;
    _gsvPending = true;
}

void Pump::onNmeaComplete() {
    _nmeaAcc[_nmeaLen] = 0;
    const char* line = reinterpret_cast<const char*>(_nmeaAcc);
    if (!LC29H_GNSS::hasValidNmeaChecksum(line)) {
        ++_checksumFails;
        _nmeaLen = 0;
        _inNmea = false;
        return;
    }
    const Kind kind = classify(line);
    if (kind == Kind::Gsv) {
        postGsv(line);
    } else if (kind != Kind::Other) {
        postLine(kind, line);
    }
    _nmeaLen = 0;
    _inNmea = false;
}

void Pump::onRtcmComplete() {
    if (_rtcmCount >= kRtcmFifo) {
        _rtcmHead = static_cast<uint8_t>((_rtcmHead + 1U) % kRtcmFifo);
        --_rtcmCount;
        ++_rtcmDrops;
    }
    const uint8_t slot = static_cast<uint8_t>((_rtcmHead + _rtcmCount) % kRtcmFifo);
    _rtcmQ[slot].len = static_cast<uint16_t>(_rtcmExpected);
    memcpy(_rtcmQ[slot].data, _rtcmAcc, _rtcmExpected);
    ++_rtcmCount;
    _rtcmLen = 0;
    _rtcmExpected = 0;
    _rtcmPhase = 0;
}

void Pump::frame() {
    uint8_t by = 0;
    while (popDrainByte(by)) {
        if (_inNmea) {
            if (by == '\n') {
                if (_nmeaLen > 0) {
                    onNmeaComplete();
                } else {
                    _inNmea = false;
                    _nmeaLen = 0;
                }
                continue;
            }
            if (by == '\r') {
                continue;
            }
            if (by < 0x20 || by > 0x7E || _nmeaLen + 1 >= kLineMax) {
                ++_resyncs;
                _inNmea = false;
                _nmeaLen = 0;
                if (by == 0xD3) {
                    _rtcmAcc[0] = by;
                    _rtcmLen = 1;
                    _rtcmPhase = 1;
                } else if (by == '$' || by == '!') {
                    _nmeaAcc[0] = by;
                    _nmeaLen = 1;
                    _inNmea = true;
                }
                continue;
            }
            _nmeaAcc[_nmeaLen++] = by;
            continue;
        }

        if (_rtcmPhase > 0) {
            if (_rtcmLen >= kRtcmCap) {
                _rtcmPhase = 0;
                _rtcmLen = 0;
                ++_resyncs;
                continue;
            }
            _rtcmAcc[_rtcmLen++] = by;
            if (_rtcmPhase == 1) {
                _rtcmPhase = 2;
                continue;
            }
            if (_rtcmPhase == 2) {
                const size_t payloadLen =
                    ((static_cast<size_t>(_rtcmAcc[1]) & 0x03U) << 8U) |
                    static_cast<size_t>(_rtcmAcc[2]);
                _rtcmExpected = payloadLen + 6U;
                if (_rtcmExpected > kRtcmCap || _rtcmExpected < 6U) {
                    _rtcmPhase = 0;
                    _rtcmLen = 0;
                    ++_resyncs;
                    continue;
                }
                _rtcmPhase = 3;
                continue;
            }
            if (_rtcmPhase == 3 && _rtcmLen >= _rtcmExpected) {
                onRtcmComplete();
            }
            continue;
        }

        if (by == '$' || by == '!') {
            _nmeaAcc[0] = by;
            _nmeaLen = 1;
            _inNmea = true;
            continue;
        }
        if (by == 0xD3) {
            _rtcmAcc[0] = by;
            _rtcmLen = 1;
            _rtcmPhase = 1;
        }
    }
}

bool Pump::deliverSlot(LineSlot& slot, NmeaHandler nmea, void* user) {
    if (!slot.pending || nmea == nullptr) {
        return false;
    }
    nmea(slot.line, user);
    slot.pending = false;
    slot.skipped = 0;
    return true;
}

uint8_t Pump::deliverGsv(NmeaHandler nmea, void* user) {
    if (!_gsvPending || nmea == nullptr || _gsvCount == 0) {
        return 0;
    }
    nmea(_gsvLines[0], user);
    for (uint8_t i = 1; i < _gsvCount; ++i) {
        memcpy(_gsvLines[i - 1], _gsvLines[i], kLineMax);
    }
    --_gsvCount;
    if (_gsvCount == 0) {
        _gsvPending = false;
        _gsvDue = false;
        _gsvSkipped = 0;
    }
    return 1;
}

uint8_t Pump::processRtcm(uint32_t budgetMs, RtcmHandler rtcm, void* user) {
    const uint32_t started = millis();
    uint8_t delivered = 0;
    while (_rtcmCount > 0) {
        const RtcmSlot& slot = _rtcmQ[_rtcmHead];
        if (rtcm != nullptr && slot.len > 0) {
            rtcm(slot.data, slot.len, user);
        }
        _rtcmHead = static_cast<uint8_t>((_rtcmHead + 1U) % kRtcmFifo);
        --_rtcmCount;
        ++delivered;
        if ((millis() - started) >= budgetMs) {
            return delivered;
        }
    }
    return delivered;
}

uint8_t Pump::processNmea(uint32_t budgetMs, NmeaHandler nmea, void* user) {
    const uint32_t started = millis();
    uint8_t delivered = 0;

    if (deliverSlot(_rmc, nmea, user)) {
        ++delivered;
    }
    if (deliverSlot(_svin, nmea, user)) {
        ++delivered;
    }
    if (deliverSlot(_gga, nmea, user)) {
        ++delivered;
    }
    // budget 0 still delivered needed status above. Extras wait for leftover time.
    if (budgetMs == 0 || (millis() - started) >= budgetMs) {
        return delivered;
    }

    if (_gsvDue || ((_pri.gsv == Priority::Needed) && _gsvPending)) {
        delivered = static_cast<uint8_t>(delivered + deliverGsv(nmea, user));
    }
    if ((millis() - started) >= budgetMs) {
        return delivered;
    }

    if (deliverSlot(_gst, nmea, user)) {
        ++delivered;
    }
    if (deliverSlot(_gsa, nmea, user)) {
        ++delivered;
    }
    if (deliverSlot(_epe, nmea, user)) {
        ++delivered;
    }
    while (_gsvPending && (millis() - started) < budgetMs) {
        delivered = static_cast<uint8_t>(delivered + deliverGsv(nmea, user));
    }
    return delivered;
}

uint8_t Pump::process(uint32_t budgetMs, RtcmHandler rtcm, NmeaHandler nmea, void* user) {
    const uint32_t started = millis();
    uint8_t delivered = processRtcm(budgetMs, rtcm, user);
    const uint32_t used = millis() - started;
    const uint32_t remain = (used >= budgetMs) ? 0 : (budgetMs - used);
    delivered = static_cast<uint8_t>(delivered + processNmea(remain, nmea, user));
    return delivered;
}

}  // namespace LC29H_UartPump

#endif  // ARDUINO_ARCH_ESP32
